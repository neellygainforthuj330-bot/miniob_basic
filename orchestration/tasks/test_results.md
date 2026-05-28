regression: pass
new_feature: pass

details:
  [regression] primary-aggregation-func: passed
  [regression] primary-date: passed
  [regression] primary-drop-table: passed
  [regression] primary-update: passed
  [regression] primary-order-by: passed

  [new] primary-join-tables: PASSED (all sections)
    sections 1-3 (small joins, empty joins): PASSED
    section 4 (6-table × 100-row large join): PASSED (~2s, was timeout)
      - Fix: interleaved join predicates between JoinLogicalOperator levels
        in logical_plan_generator.cpp, reducing intermediate tuples from
        ~9 billion to ~10,000
      - Also eliminated constant-only tautologies like 1=1 as join filters
