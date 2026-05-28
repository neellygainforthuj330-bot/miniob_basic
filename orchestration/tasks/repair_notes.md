## 修复 #1

失败用例: primary-aggregation-func (all queries)
失败类别: 逻辑错误
根因: AggregationPhysicalOperator::next() 在首次计算完成后将 returned_ 设为 false，导致第二次调用时重新进入 computed_ 分支并再次返回 SUCCESS，每行数据被输出两次。

定位:
  文件: src/observer/sql/operator/aggregation_physical_operator.cpp
  行号: 161
  问题: `returned_ = false;` 应为 `returned_ = true;`
        第一次 next() 计算完成后应直接标记为已返回，不应给第二次返回 SUCCESS 的机会。

修复方向:
  将 aggregation_physical_operator.cpp:161 的 returned_ = false 改为 returned_ = true。
  同时可考虑简化逻辑：删除 computed_ 中间检查块（25-30行），仅保留 returned_ 标志位控制。

验证 SQL:
  SELECT count(*) FROM aggregation_func;
  期望输出: COUNT(*)\n4  (仅一行数据，非两行)

优先级: 阻塞
<!-- repair_agent: done, issues: 1, blockers: 1 -->

## 修复 #2

失败用例: primary-join-tables section 4 (large join timeout)
失败类别: 逻辑错误（性能超时）
根因: 所有跨表连接条件（equi-join）被聚合成单个 PredicateLogicalOperator 放在整个 6 层 JoinLogicalOperator 树的最顶端。NestedLoopJoinPhysicalOperator 是纯笛卡尔积算子，不执行任何过滤。在执行任何过滤之前，必须先物化约 90 亿（9×10^9）个中间元组，导致超时。

定位:
  文件: src/observer/sql/optimizer/logical_plan_generator.cpp
  行号: 92-117（filter 分类）、120-159（Join 树构建）、161-178（顶层 predicate 创建）
  问题:
    1. filter 分类逻辑将所有跨表条件和常量-常量条件都归入 join_filters
       - `1 = 1` → 两侧都是常量（ref_table 为空）→ join_filters
    2. 所有 join_filters 打包成一个 PredicateLogicalOperator 放在 join 树最顶端
    3. NestedLoopJoin 必须先产生所有 90 亿组合，然后 predicate 才过滤到 90 行

  文件: src/observer/sql/operator/join_physical_operator.cpp
  行号: 38-66（next 方法）
  问题: NestedLoopJoinPhysicalOperator 不持有任何 join 条件，next() 是纯笛卡尔积

修复方向（方案 A — 在 Join 层级之间交错插入 predicate）:

  修改 logical_plan_generator.cpp 的 create_plan(SelectStmt) 方法：

  a) 扩展 filter 分类逻辑：对于 join_filters 中的每个单元，收集其涉及的所有 table name。
     若涉及 0 个表（两侧均为常量如 `1=1`）→ 直接丢弃（恒为 true，无需过滤）。

  b) 在构建 join 树循环中跟踪已加入的表集合 joined_tables。每次创建 JoinLogicalOperator 后，
     检查 join_filters 中哪些 filter 的所有引用表都在 joined_tables 中 → 将这些 filter 转为
     PredicateLogicalOperator 插入到当前 Join 节点之上，包装 table_oper。

  c) 兜底：若循环后仍有剩余 join_filters（跨非连续 join 层级），保留原顶层 predicate 创建逻辑。

  交错后的计划树（6 表示例）:
    Project
      Join(L5, L6)
        Predicate(large_5.id = large_6.id)
          Join(L4, L5)                    ← 1=1 已消除
            Join(L3, L4)
              Predicate(large_3.id = large_4.id)
                Join(L1+L2, L3)
                  Predicate(large_1.id = large_3.id)
                    Join(L1, L2)
                      Predicate(large_1.id = large_2.id)
                        TableGet(L1)  TableGet(L2)

  最大中间元组数从 ~90 亿降至 ~10,000。

验证 SQL:
  select * from join_table_large_1 inner join join_table_large_2 on join_table_large_1.id=join_table_large_2.id inner join join_table_large_3 on join_table_large_1.id=join_table_large_3.id inner join join_table_large_4 on join_table_large_3.id=join_table_large_4.id inner join join_table_large_5 on 1=1 inner join join_table_large_6 on join_table_large_5.id=join_table_large_6.id where join_table_large_3.num3 <10 and join_table_large_5.num5>90;
  期望输出: 90 行数据（参考 primary-join-tables.result 第 1285-1375 行）

优先级: 阻塞
<!-- repair_agent: done, issues: 2, blockers: 2 -->
