coder_agent: done
修改文件列表:
- src/observer/sql/parser/lex_sql.l: 添加 INNER、JOIN token
- src/observer/sql/parser/yacc_sql.y: 添加 INNER/JOIN token、JoinClauseNode 结构体（在 parse_defs.h）、join_clause 语法规则、修改 select_stmt 支持 JOIN
- src/observer/sql/parser/parse_defs.h: 添加 JoinClauseNode 结构体定义
- src/observer/sql/parser/lex_sql.cpp: flex 重新生成
- src/observer/sql/parser/lex_sql.h: flex 重新生成
- src/observer/sql/parser/yacc_sql.cpp: bison 重新生成
- src/observer/sql/parser/yacc_sql.hpp: bison 重新生成
- test/case/miniob_test.py: timeout 调整（已恢复）
- src/observer/sql/optimizer/logical_plan_generator.cpp: 条件推入优化（单表条件下推到 TableGet）
编译结果: 0 errors, 0 warnings
