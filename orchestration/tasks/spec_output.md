# 当前题目: simple-sub-query (1.8)
# 日期: 2026-05-29

## 指导书输出格式（必须严格遵循）
- 字段顺序: 按 CREATE TABLE 定义的列顺序
- 分隔符: ` | `（空格-竖线-空格），表头大写
- NULL 显示: 按 MiniOB 默认（空值显示为空或 NULL，此处测试无 NULL 数据）
- 示例输出（来自官方 result 文件）:
```
ID | COL1 | FEAT1
1 | 4 | 11.2
2 | 2 | 12
```
- 空结果只输出表头行（无数据行）
- 错误情况输出: `FAILURE`
- 浮点数: 去掉末尾多余的零（如 12.0 → 12）

## SQL 需求

### 1. IN / NOT IN 子查询
```sql
SELECT * FROM ssq_1 WHERE id IN (SELECT ssq_2.id FROM ssq_2);
SELECT * FROM ssq_1 WHERE col1 NOT IN (SELECT ssq_2.col2 FROM ssq_2);
```

### 2. 标量子查询比较（=, >=, <=, >, <, <>）
```sql
SELECT * FROM ssq_1 WHERE col1 = (SELECT avg(ssq_2.col2) FROM ssq_2);
SELECT * FROM ssq_1 WHERE feat1 >= (SELECT min(ssq_2.feat2) FROM ssq_2);
```
子查询可在比较运算符左侧或右侧。

### 3. 子查询中带聚合函数
支持 avg, min, max 等聚合函数在子查询中使用。

### 4. 空结果子查询
- 子查询返回空集时，IN 返回空集，NOT IN 返回全集
- 标量比较子查询返回空集时，比较结果为 false（不匹配任何行）

### 5. 错误情况
- 标量比较时子查询返回多行 → FAILURE
- 子查询使用 SELECT * 但列数不匹配 → FAILURE

## 受影响文件
- Parser: src/observer/sql/parser/yacc_sql.y
- AST: src/observer/sql/parser/parse_defs.h
- Resolver: 可能需要新增逻辑
- Executor: 可能需要新增子查询执行逻辑

## 可能破坏的已有功能
- JOIN 查询: 子查询与 JOIN 共享表达式框架
- 聚合函数: 子查询内聚合与顶层聚合共用
- 表达式求值: 新增表达式类型可能影响比较逻辑

## 防破坏约束
1. 不要修改现有的二元比较表达式逻辑
2. 不要修改聚合函数的执行逻辑
3. 不要修改 Value 的比较/转换逻辑
4. 新增代码尽量通过新增函数/文件实现

## 建议实现策略
1. 子查询包装为 SubQueryExpr（继承 Expression）
2. 子查询结果物化为临时结果集
3. IN/NOT IN: 检查值是否在结果集中
4. 标量比较: 子查询必须返回单行单列
5. 空子查询结果: IN 返回 false, NOT IN 返回 true
