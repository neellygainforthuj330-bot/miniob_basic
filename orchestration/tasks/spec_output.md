# 当前题目: join-tables
# 日期: 2026-05-28

## 指导书输出格式（必须严格遵循）

指导书（1.7节）未给出具体输出示例。输出格式以 `test/case/result/primary-join-tables.result` 为准：

- **列分隔符**: ` | `（空格-管道-空格）
- **行顺序**: 数据行在前，列名/表头行在后
- **列名格式**: `TABLE_NAME.COLUMN_NAME`（全大写，点号连接）
- **NULL 显示**: `NULL`（全大写）
- **空结果集**: 仅输出表头行，无数据行

示例输出（来自 primary-join-tables.result）:
```
Select * from join_table_1 inner join join_table_2 on join_table_1.id=join_table_2.id;
1 | A | 1 | 2
2 | B | 2 | 15
JOIN_TABLE_1.ID | JOIN_TABLE_1.NAME | JOIN_TABLE_2.ID | JOIN_TABLE_2.NUM
```

## SQL 需求

### 语法
```
SELECT ... FROM t1 INNER JOIN t2 ON cond [INNER JOIN t3 ON cond ...] [WHERE where_cond]
```
- INNER JOIN 关键字必须支持
- ON 子句定义连接条件
- 支持多条 ON 条件用 AND 连接
- WHERE 子句与 JOIN 组合使用
- 支持链式多表 JOIN

### 测试用例覆盖（来自 primary-join-tables.test）

1. 基础两表 join: `SELECT * FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id`
2. 投影 join: `SELECT join_table_1.name FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id`
3. 三表链式 join: `SELECT * FROM t1 INNER JOIN t2 ON t1.id=t2.id INNER JOIN t3 ON t1.id=t3.id`
4. 复合 ON + WHERE: `SELECT * FROM t1 INNER JOIN t2 ON t1.id=t2.id AND t2.num>13 WHERE t1.name='b'`
5. 空表 join: 与空表 INNER JOIN 返回空结果集
6. 大 join: 6 表连锁 + 复杂条件

## 受影响文件

- **yacc_sql.y**: 新增 INNER、JOIN、ON 关键字和 join 语法规则
- **lex_sql.l**: 新增 INNER、JOIN、ON token
- **parse_defs.h**: 可能需要新增 join AST 节点
- **select_stmt.h/cpp**: 存储 join 条件
- **optimizer/execute_stage**: 可能需要创建 join 算子
- **physical_operator.h/cpp**: 可能新增 join 类型

## 可能破坏的已有功能

- **select-tables**: 隐式多表查询（逗号连接）不应受影响
- **order-by**: JOIN 结果排序不受影响
- **aggregation-func**: JOIN 结果聚合不受影响
- **WHERE 条件**: WHERE 与 ON 条件需正确组合

## 防破坏约束

1. 不修改隐式多表连接（逗号连接）的语义
2. 不修改 WHERE 子句解析和求值逻辑
3. INNER JOIN 的 ON 条件本质上等同于 WHERE 条件，可将其推入 filter
4. 输出格式与 primary-join-tables.result 完全一致
5. 不修改已有函数签名
6. 列头输出格式（数据在前、表头在后）保持不变

## 建议实现策略

最小修改方案：
1. Parser: yacc_sql.l 添加 INNER/JOIN/ON token，yacc_sql.y 添加语法规则将 SQL `INNER JOIN t2 ON cond` 转化为内部表示
2. 将 ON 条件合并到 WHERE 条件中（INNER JOIN 语义下等价）
3. 复用现有多表扫描 + 条件过滤机制
4. 不需要新物理算子，join 条件通过 filter 算子处理
