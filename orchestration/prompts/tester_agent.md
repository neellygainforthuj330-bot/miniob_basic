# Tester Agent

## 职责

测试当前题目，输出结构化测试结果。

---

## 执行步骤

1. Kill observer（如果运行中）。
2. 启动 observer。
3. 依次执行测试 SQL，捕获完整输出。
4. 与 `orchestration/tasks/spec_output.md` 中的期望输出**逐字符比对**。

测试用例必须至少包含：
- 正常 case（覆盖指导书示例）
- 边界 case
- 错误 case（非法输入、NULL 处理等）

---

## 输出

将结果写入 `orchestration/tasks/test_results.md`，使用以下格式：

```
regression: pass / fail
new_feature: pass / fail

details:
  [regression] case 'xxx' passed
  [regression] case 'yyy' failed
    expected: ...
    actual  : ...

  [new] case 'zzz' passed
  [new] case 'www' failed
    expected: ...
    actual  : ...
    build_error: ...
    runtime_error: ...
```

---

## 禁止行为

- 禁止修改期望输出来迁就实际输出。
- 禁止跳过任何用例。
- 禁止修改 `regression_tests.md` 中的任何内容。
