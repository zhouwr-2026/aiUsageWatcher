import QtQuick
import QtTest
import "../package/contents/js/scriptTools.js" as ScriptTools

Item {
    TestCase {
        name: "ScriptTools"

        function test_contract_accepts_requested_quota_variables() {
            const result = ScriptTools.validateContract(ScriptTools.DEFAULT_SCRIPT, [{
                planName: "5小时",
                usedVariable: "${used}",
                limitVariable: "${limit}"
            }])
            verify(result.valid, result.message)
            compare(result.variables.join(","), "used,limit")
        }

        function test_contract_rejects_missing_result_variable() {
            const result = ScriptTools.validateContract(ScriptTools.DEFAULT_SCRIPT, [{
                planName: "月限额",
                usedVariable: "${monthlyUsed}",
                limitVariable: "${limit}"
            }])
            verify(!result.valid)
            verify(result.message.indexOf("monthlyUsed") >= 0)
        }

        function test_contract_allows_missing_optional_reset_variable() {
            const result = ScriptTools.validateContract(ScriptTools.DEFAULT_SCRIPT, [{
                planName: "月限额",
                usedVariable: "${used}",
                limitVariable: "${limit}",
                resetVariable: "${resetAt}"
            }])
            verify(result.valid, result.message)
            verify(result.variables.indexOf("resetAt") >= 0)
        }

        function test_formatter_indents_common_script_shape() {
            const formatted = ScriptTools.formatJavaScript(
                "({\nrequest: {\nurl: 'https://example.com'\n},\nextractor: function(response) {\nreturn { used: 1, limit: 2 }\n}\n})")
            verify(formatted.indexOf("    request:") >= 0)
            verify(formatted.indexOf("        url:") >= 0)
            compare(formatted[formatted.length - 1], "\n")
        }

        function test_line_numbers_follow_text_lines() {
            compare(ScriptTools.lineNumbers("one\ntwo\nthree"), "1\n2\n3")
            compare(ScriptTools.lineNumbers(""), "1")
        }
    }
}
