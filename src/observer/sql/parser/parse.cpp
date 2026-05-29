/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Meiyi
//

#include <mutex>
#include "sql/parser/parse.h"
#include "common/log/log.h"
#include "sql/expr/expression.h"
RC parse(char *st, ParsedSqlNode *sqln);

CalcSqlNode::~CalcSqlNode()
{
  for (Expression *expr : expressions) {
    delete expr;
  }
  expressions.clear();
}

ConditionSqlNode::~ConditionSqlNode()
{
  delete left_expr;
  delete right_expr;
  left_expr = nullptr;
  right_expr = nullptr;
}

ConditionSqlNode &ConditionSqlNode::operator=(ConditionSqlNode &&other) noexcept
{
  if (this != &other) {
    delete left_expr; delete right_expr;
    left_is_attr = other.left_is_attr; left_value = std::move(other.left_value);
    left_attr = std::move(other.left_attr); comp = other.comp;
    right_is_attr = other.right_is_attr; right_attr = std::move(other.right_attr);
    right_value = std::move(other.right_value);
    left_expr = other.left_expr; right_expr = other.right_expr;
    other.left_expr = nullptr; other.right_expr = nullptr;
  }
  return *this;
}

ParsedSqlNode::ParsedSqlNode() : flag(SCF_ERROR)
{}

ParsedSqlNode::ParsedSqlNode(SqlCommandFlag _flag) : flag(_flag)
{}

void ParsedSqlResult::add_sql_node(std::unique_ptr<ParsedSqlNode> sql_node)
{
  sql_nodes_.emplace_back(std::move(sql_node));
}

////////////////////////////////////////////////////////////////////////////////

int sql_parse(const char *st, ParsedSqlResult *sql_result);

RC parse(const char *st, ParsedSqlResult *sql_result)
{
  sql_parse(st, sql_result);
  return RC::SUCCESS;
}
