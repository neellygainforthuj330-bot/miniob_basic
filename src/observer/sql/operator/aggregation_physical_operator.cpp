#include "sql/operator/aggregation_physical_operator.h"
#include "storage/record/record.h"
#include "storage/table/table.h"
#include "common/log/log.h"
#include <limits>

void AggregationPhysicalOperator::add_aggregation(AggregationType agg_type, const Table *table,
    const FieldMeta *field_meta, const std::string &alias)
{
  AggregationField agg;
  agg.agg_type = agg_type;
  agg.table = table;
  agg.field_meta = field_meta;
  agg.alias = alias;
  agg_fields_.push_back(agg);
}

RC AggregationPhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) return RC::SUCCESS;
  return children_[0]->open(trx);
}

RC AggregationPhysicalOperator::next()
{
  if (returned_) return RC::RECORD_EOF;
  if (computed_) {
    returned_ = true;
    return RC::SUCCESS;
  }

  if (children_.empty()) return RC::RECORD_EOF;
  PhysicalOperator *child = children_[0].get();

  int n = agg_fields_.size();
  std::vector<double> sum_vals(n, 0);
  std::vector<double> max_vals(n, std::numeric_limits<double>::lowest());
  std::vector<double> min_vals(n, std::numeric_limits<double>::max());
  std::vector<int> counts(n, 0);

  RC rc = RC::SUCCESS;
  while ((rc = child->next()) == RC::SUCCESS) {
    Tuple *tuple = child->current_tuple();
    for (int i = 0; i < n; i++) {
      AggregationField &af = agg_fields_[i];
      if (af.agg_type == AGG_COUNT && af.field_meta == nullptr) {
        counts[i]++;
        continue;
      }
      if (af.field_meta == nullptr) continue;

      Value val;
      TupleCellSpec spec(af.table->name(), af.field_meta->name(), af.field_meta->name());
      rc = tuple->find_cell(spec, val);
      if (rc != RC::SUCCESS) continue;

      double v = 0;
      if (val.attr_type() == INTS) v = val.get_int();
      else if (val.attr_type() == FLOATS) v = val.get_float();
      else if (af.agg_type == AGG_COUNT) { counts[i]++; continue; }
      else continue;

      sum_vals[i] += v;
      if (v > max_vals[i]) max_vals[i] = v;
      if (v < min_vals[i]) min_vals[i] = v;
      counts[i]++;
    }
  }

  std::vector<Value> results;
  std::vector<std::string> aliases;

  for (int i = 0; i < n; i++) {
    AggregationField &af = agg_fields_[i];
    Value result;
    switch (af.agg_type) {
      case AGG_COUNT:
        result.set_int(counts[i]);
        break;
      case AGG_SUM:
        if (af.field_meta && af.field_meta->type() == INTS)
          result.set_int((int)sum_vals[i]);
        else
          result.set_float((float)sum_vals[i]);
        break;
      case AGG_AVG:
        if (counts[i] > 0)
          result.set_float((float)(sum_vals[i] / counts[i]));
        else
          result.set_float(0);
        break;
      case AGG_MAX:
        if (af.field_meta && af.field_meta->type() == INTS)
          result.set_int((int)max_vals[i]);
        else
          result.set_float((float)max_vals[i]);
        break;
      case AGG_MIN:
        if (af.field_meta && af.field_meta->type() == INTS)
          result.set_int((int)min_vals[i]);
        else
          result.set_float((float)min_vals[i]);
        break;
      default:
        result.set_int(0);
    }
    results.push_back(result);
    aliases.push_back(af.alias);
  }

  result_tuple_.set_cells(results);
  result_tuple_.set_aliases(aliases);

  computed_ = true;
  returned_ = false;
  return RC::SUCCESS;
}

RC AggregationPhysicalOperator::close()
{
  if (!children_.empty()) children_[0]->close();
  return RC::SUCCESS;
}

Tuple *AggregationPhysicalOperator::current_tuple()
{
  return &result_tuple_;
}