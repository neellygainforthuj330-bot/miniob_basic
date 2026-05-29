/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/rc.h"
#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/optimizer/logical_plan_generator.h"
#include "sql/optimizer/physical_plan_generator.h"
#include "sql/operator/aggregation_physical_operator.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "session/session.h"

FilterStmt::~FilterStmt()
{
  for (FilterUnit *unit : filter_units_) {
    delete unit;
  }
  filter_units_.clear();
}

RC FilterStmt::create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    const ConditionSqlNode *conditions, int condition_num, FilterStmt *&stmt)
{
  RC rc = RC::SUCCESS;
  stmt = nullptr;

  FilterStmt *tmp_stmt = new FilterStmt();
  for (int i = 0; i < condition_num; i++) {
    FilterUnit *filter_unit = nullptr;
    rc = create_filter_unit(db, default_table, tables, conditions[i], filter_unit);
    if (rc != RC::SUCCESS) {
      delete tmp_stmt;
      LOG_WARN("failed to create filter unit. condition index=%d", i);
      return rc;
    }
    tmp_stmt->filter_units_.push_back(filter_unit);
  }

  stmt = tmp_stmt;
  return rc;
}

RC get_table_and_field(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    const RelAttrSqlNode &attr, Table *&table, const FieldMeta *&field)
{
  if (common::is_blank(attr.relation_name.c_str())) {
    table = default_table;
  } else if (nullptr != tables) {
    auto iter = tables->find(attr.relation_name);
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    table = db->find_table(attr.relation_name.c_str());
  }
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  field = table->table_meta().field(attr.attribute_name.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name.c_str());
    table = nullptr;
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }
  return RC::SUCCESS;
}

// 解析一个表达式侧（left 或 right），如果结果是简单类型则转换为 FilterObj
static RC resolve_side(Expression *&expr, Table *default_table,
                       std::unordered_map<std::string, Table *> *tables,
                       FilterObj &obj, bool &is_simple)
{
  if (expr == nullptr) return RC::SUCCESS;
  std::unique_ptr<Expression> e(expr);
  expr = nullptr; // 转移所有权
  RC rc = resolve_expression(e, default_table, tables);
  if (rc != RC::SUCCESS) return rc;
  if (e->type() == ExprType::FIELD) {
    obj.init_attr(static_cast<FieldExpr *>(e.get())->field());
    is_simple = true;
    return RC::SUCCESS;
  } else if (e->type() == ExprType::VALUE) {
    Value v;
    e->try_get_value(v);
    obj.init_value(v);
    is_simple = true;
    return RC::SUCCESS;
  }
  // 复杂表达式: 放回 expr 指针，稍后存入 FilterUnit
  is_simple = false;
  expr = e.release();
  return RC::SUCCESS;
}

RC FilterStmt::create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    const ConditionSqlNode &condition, FilterUnit *&filter_unit)
{
  RC rc = RC::SUCCESS;

  CompOp comp = condition.comp;
if (comp < EQUAL_TO || comp >= NO_OP) {
    LOG_WARN("invalid compare operator : %d", comp);
    return RC::INVALID_ARGUMENT;
  }

  // 检查是否有表达式操作数
  bool has_expr = (condition.left_expr != nullptr || condition.right_expr != nullptr);

  if (!has_expr) {
    // === 纯 legacy 路径 ===
    filter_unit = new FilterUnit;
    filter_unit->set_comp(comp);
    if (condition.left_is_attr) {
      Table *table = nullptr;
      const FieldMeta *field = nullptr;
      rc = get_table_and_field(db, default_table, tables, condition.left_attr, table, field);
      if (rc != RC::SUCCESS) { delete filter_unit; return rc; }
      FilterObj obj; obj.init_attr(Field(table, field));
      filter_unit->set_left(obj);
    } else {
      FilterObj obj; obj.init_value(condition.left_value);
      filter_unit->set_left(obj);
    }
    if (condition.right_is_attr) {
      Table *table = nullptr;
      const FieldMeta *field = nullptr;
      rc = get_table_and_field(db, default_table, tables, condition.right_attr, table, field);
      if (rc != RC::SUCCESS) { delete filter_unit; return rc; }
      FilterObj obj; obj.init_attr(Field(table, field));
      filter_unit->set_right(obj);
    } else {
      FilterObj obj; obj.init_value(condition.right_value);
      filter_unit->set_right(obj);
    }
  } else {
    // === 表达式路径：解析两边，尝试简化为 legacy ===
    FilterObj left_obj, right_obj;
    bool left_simple = false, right_simple = false;

    // 转移并解析 left
    Expression *left_e = condition.left_expr;
    if (left_e) const_cast<ConditionSqlNode &>(condition).left_expr = nullptr;
    rc = resolve_side(left_e, default_table, tables, left_obj, left_simple);
    if (rc != RC::SUCCESS) return rc;

    // 转移并解析 right
    Expression *right_e = condition.right_expr;
    if (right_e) const_cast<ConditionSqlNode &>(condition).right_expr = nullptr;
    rc = resolve_side(right_e, default_table, tables, right_obj, right_simple);
    if (rc != RC::SUCCESS) { delete left_e; return rc; }

    // Check for subquery in right side
    bool right_is_subquery = (right_e != nullptr && right_e->type() == ExprType::SUBQUERY);
    bool left_is_subquery  = (left_e  != nullptr && left_e->type()  == ExprType::SUBQUERY);
    bool is_in_op    = (comp == IN_OP || comp == NOT_IN);
    bool is_exists_op = (comp == EXISTS_OP || comp == NOT_EXISTS);

    if (right_is_subquery || left_is_subquery) {
      auto *sq = static_cast<SubQueryExpr *>(right_is_subquery ? right_e : left_e);
      auto *other_expr = right_is_subquery ? left_e : right_e;

      // 执行子查询
      LOG_INFO("subquery: executing subquery, relations count=%d", (int)sq->select_node()->relations.size());
      Stmt *inner_stmt = nullptr;
      rc = SelectStmt::create(db, *sq->select_node(), inner_stmt);
      if (rc != RC::SUCCESS) {
        LOG_ERROR("subquery: failed to create subquery stmt. rc=%s", strrc(rc));
        delete left_e; delete right_e;
        return rc;
      }
      auto *sel_stmt = static_cast<SelectStmt *>(inner_stmt);
      LOG_INFO("subquery: stmt created, tables=%d", (int)sel_stmt->tables().size());

      LogicalPlanGenerator logic_gen;
      std::unique_ptr<LogicalOperator> logic_oper;
      rc = logic_gen.create(sel_stmt, logic_oper);
      if (rc != RC::SUCCESS) {
        LOG_ERROR("subquery: failed to create subquery logical plan. rc=%s", strrc(rc));
        delete sel_stmt; delete left_e; delete right_e;
        return rc;
      }
      LOG_INFO("subquery: logical plan created");

      PhysicalPlanGenerator phys_gen;
      std::unique_ptr<PhysicalOperator> phys_oper;
      rc = phys_gen.create(*logic_oper, phys_oper);
      if (rc != RC::SUCCESS) {
        LOG_ERROR("subquery: failed to create subquery physical plan. rc=%s", strrc(rc));
        delete sel_stmt; delete left_e; delete right_e;
        return rc;
      }
      LOG_INFO("subquery: physical plan created");

      // 聚合查询包一层聚合算子（与 optimize_stage.cpp 一致）
      if (sel_stmt->has_aggregation()) {
        AggregationPhysicalOperator *agg_oper = new AggregationPhysicalOperator();
        for (const AggregationField &af : sel_stmt->agg_fields()) {
          agg_oper->add_aggregation(af.agg_type, af.table, af.field_meta, af.alias);
        }
        agg_oper->add_child(std::move(phys_oper));
        phys_oper.reset(agg_oper);
      }

      Session *session = Session::current_session();
      Trx *trx = session ? session->current_trx() : nullptr;
      LOG_INFO("subquery: session=%p trx=%p", session, trx);
      rc = phys_oper->open(trx);
      if (rc != RC::SUCCESS) {
        LOG_ERROR("subquery: failed to open subquery operator. rc=%s", strrc(rc));
        delete sel_stmt; delete left_e; delete right_e;
        return rc;
      }
      LOG_INFO("subquery: operator opened, collecting results");

      // 收集子查询结果的第一列
      std::vector<Value> result_values;
      int row_count = 0;
      while ((rc = phys_oper->next()) == RC::SUCCESS) {
        Tuple *tuple = phys_oper->current_tuple();
        if (tuple == nullptr) break;
        row_count++;
        Value v;
        rc = tuple->cell_at(0, v);
        if (rc == RC::SUCCESS) {
          result_values.push_back(v);
        }
      }
      phys_oper->close();
      delete sel_stmt;
      rc = RC::SUCCESS;  // 子查询执行成功，重置 rc（while 循环结束后 rc 是 EOF）

      if (is_exists_op) {
        // EXISTS / NOT EXISTS
        bool has_rows = (row_count > 0);
        if (comp == EXISTS_OP) {
          if (has_rows) {
            filter_unit = new FilterUnit;
            filter_unit->set_custom_expr(new ValueExpr(Value(true)));
          } else {
            filter_unit = new FilterUnit;
            filter_unit->set_custom_expr(new ValueExpr(Value(false)));
          }
        } else {
          if (!has_rows) {
            filter_unit = new FilterUnit;
            filter_unit->set_custom_expr(new ValueExpr(Value(true)));
          } else {
            filter_unit = new FilterUnit;
            filter_unit->set_custom_expr(new ValueExpr(Value(false)));
          }
        }
        delete left_e; delete right_e;
        return rc;
      }

      if (is_in_op) {
        // 检查子查询是否使用了 SELECT *（多列），IN/NOT IN 只允许单列
        const auto &sel_node = *sq->select_node();
        bool has_star = false;
        int col_count = 0;
        if (!sel_node.attributes.empty()) {
          for (auto &a : sel_node.attributes) {
            if (a.attribute_name == "*") { has_star = true; break; }
          }
          col_count = (int)sel_node.attributes.size();
        }
        if (!sel_node.expressions.empty()) {
          col_count = (int)sel_node.expressions.size();
        }
        if (has_star || col_count > 1) {
          delete left_e; delete right_e;
          return RC::INVALID_ARGUMENT;
        }

        // IN / NOT IN — rebuild the left-side expression from FilterObj
        auto make_left_expr = [&]() -> std::unique_ptr<Expression> {
          const FilterObj &obj = right_is_subquery ? left_obj : right_obj;
          if (obj.is_attr) {
            return std::unique_ptr<Expression>(new FieldExpr(obj.field));
          } else {
            return std::unique_ptr<Expression>(new ValueExpr(obj.value));
          }
        };
        if (result_values.empty()) {
          bool empty_is_true = (comp == NOT_IN);
          filter_unit = new FilterUnit;
          if (empty_is_true) {
            filter_unit->set_custom_expr(new ValueExpr(Value(true)));
          } else {
            filter_unit->set_custom_expr(new ValueExpr(Value(false)));
          }
        } else if (comp == IN_OP) {
          std::vector<std::unique_ptr<Expression>> or_children;
          for (auto &rv : result_values) {
            or_children.emplace_back(
              new ComparisonExpr(EQUAL_TO, make_left_expr(),
                std::unique_ptr<Expression>(new ValueExpr(rv))));
          }
          auto *or_expr = new ConjunctionExpr(ConjunctionExpr::Type::OR, or_children);
          filter_unit = new FilterUnit;
          filter_unit->set_custom_expr(or_expr);
        } else {
          std::vector<std::unique_ptr<Expression>> and_children;
          for (auto &rv : result_values) {
            and_children.emplace_back(
              new ComparisonExpr(NOT_EQUAL, make_left_expr(),
                std::unique_ptr<Expression>(new ValueExpr(rv))));
          }
          auto *and_expr = new ConjunctionExpr(ConjunctionExpr::Type::AND, and_children);
          filter_unit = new FilterUnit;
          filter_unit->set_custom_expr(and_expr);
        }
        delete left_e; delete right_e;
        return rc;
      }

      // 标量子查询（=, <, >, <=, >=, <>）
      if (row_count > 1) {
        LOG_WARN("scalar subquery returned more than one row");
        delete left_e; delete right_e;
        return RC::INVALID_ARGUMENT;
      }
      if (result_values.empty()) {
        // 空子查询结果 → 比较永远为 false
        filter_unit = new FilterUnit;
        filter_unit->set_custom_expr(new ValueExpr(Value(false)));
        delete left_e; delete right_e;
        return RC::SUCCESS;
      }

      // 替换 SubQueryExpr 为 ValueExpr
      Value scalar_val = result_values[0];
      Expression *val_expr = new ValueExpr(scalar_val);
      if (right_is_subquery) {
        delete right_e;
        right_e = val_expr;
        right_simple = true;
        right_obj.init_value(scalar_val);
      } else {
        delete left_e;
        left_e = val_expr;
        left_simple = true;
        left_obj.init_value(scalar_val);
      }

      // 回退到正常的表达式路径
      if (left_simple && right_simple) {
        filter_unit = new FilterUnit();
        filter_unit->set_comp(comp);
        filter_unit->set_left(left_obj);
        filter_unit->set_right(right_obj);
        return rc;
      }
    }

    if (left_simple && right_simple && !left_obj.is_attr && !right_obj.is_attr) {
      // 两边都是常量: 使用表达式路径以保留常量比较语义
      filter_unit = new FilterUnit();
      filter_unit->set_comp(comp);
      filter_unit->set_left_expr(new ValueExpr(left_obj.value));
      filter_unit->set_right_expr(new ValueExpr(right_obj.value));
    } else if (left_simple && right_simple) {
      // 两边都是简单类型且至少一边是字段: 使用 legacy 路径
      filter_unit = new FilterUnit();
      filter_unit->set_comp(comp);
      filter_unit->set_left(left_obj);
      filter_unit->set_right(right_obj);
    } else {
      // 复杂表达式: 使用表达式路径
      filter_unit = new FilterUnit();
      filter_unit->set_comp(comp);
      if (!left_simple && left_e) {
        filter_unit->set_left_expr(left_e);
      } else if (left_simple) {
        if (left_obj.is_attr) {
          filter_unit->set_left_expr(new FieldExpr(left_obj.field));
        } else {
          filter_unit->set_left_expr(new ValueExpr(left_obj.value));
        }
      }
      if (!right_simple && right_e) {
        filter_unit->set_right_expr(right_e);
      } else if (right_simple) {
        if (right_obj.is_attr) {
          filter_unit->set_right_expr(new FieldExpr(right_obj.field));
        } else {
          filter_unit->set_right_expr(new ValueExpr(right_obj.value));
        }
      }
    }
  }

  // Type validation (legacy path only)
  if (!filter_unit->is_expr_based() && !filter_unit->has_custom_expr()) {
    if (filter_unit->left().is_attr && !filter_unit->right().is_attr) {
      const FieldMeta *field = filter_unit->left().field.meta();
      if (field->type() == DATES && filter_unit->right().value.attr_type() == CHARS) {
        delete filter_unit; return RC::INVALID_ARGUMENT;
      }
    } else if (!filter_unit->left().is_attr && filter_unit->right().is_attr) {
      const FieldMeta *field = filter_unit->right().field.meta();
      if (field->type() == DATES && filter_unit->left().value.attr_type() == CHARS) {
        delete filter_unit; return RC::INVALID_ARGUMENT;
      }
    }
  }

  return rc;
}
