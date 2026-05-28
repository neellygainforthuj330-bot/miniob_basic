# MiniOB 实验调度计划

<!-- ORIGIN_COMMIT: 40ec7c8b547dd2842089249adc91b7daca65b891 -->
<!-- LAST_REGRESSION: 2026-05-28, 5/5 passed -->

## 回归基线

| 功能 | 测试文件 | 状态 |
|------|----------|------|
| AGGREGATION-FUNC | primary-aggregation-func.test | ✅ pass |
| BASIC DATE | primary-date.test | ✅ pass |
| DROP TABLE | primary-drop-table.test | ✅ pass |
| UPDATE | primary-update.test | ✅ pass |

## 必做题

| 题目 | 分值 | 状态 |
|------|------|------|
| 优化buffer pool | 10 | ⏭️ SKIP (无统一测试) |
| select-meta | 10 | ✅ 已完成 |
| drop-table | 10 | ✅ 已完成 |
| update | 10 | ✅ 已完成 |
| date | 10 | ✅ 已完成 |
| select-tables | 10 | ✅ 已完成 |
| aggregation-func | 10 | ✅ 已完成 |

## 选做题

| 题目 | 分值 | 状态 |
|------|------|------|
| order-by | 10 | ✅ 已完成 |
| join-tables | 20 | ✅ 已完成 |
| insert | 10 | ⬜ 未实现 |
| unique | 10 | ⬜ 未实现 |
| null | 10 | ⬜ 未实现 |
| simple-sub-query | 10 | ⬜ 未实现 |
| multi-index | 20 | ⬜ 未实现 |
| text | 20 | ⬜ 未实现 |
| expression | 20 | ⬜ 未实现 |
| complex-sub-query | 20 | ⬜ 未实现 |
| group-by | 20 | ⬜ 未实现 |
