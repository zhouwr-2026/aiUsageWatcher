// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

var DEFAULT_SCRIPT = "({\n"
    + "  request: {\n"
    + "    url: \"https://example.com/api/usage\",\n"
    + "    method: \"GET\",\n"
    + "    headers: {}\n"
    + "  },\n"
    + "  extractor: function(response) {\n"
    + "    return {\n"
    + "      used: response.used,\n"
    + "      limit: response.limit\n"
    + "    };\n"
    + "  }\n"
    + "})\n";

function variableName(reference) {
    if (typeof reference !== "string")
        return "";
    var match = /^\$\{([A-Za-z_$][A-Za-z0-9_$]*)\}$/.exec(reference.trim());
    return match ? match[1] : "";
}

function _hasResultKey(script, name) {
    var escaped = name.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    return new RegExp("(?:[\\\"']" + escaped + "[\\\"']|\\b" + escaped
                      + ")\\s*:").test(script);
}

function validateContract(script, plans) {
    script = typeof script === "string" ? script : "";
    if (!script.trim())
        return { "valid": false, "message": "查询脚本不能为空", "variables": [] };
    if (!/\brequest\s*:/.test(script) || !/\burl\s*:/.test(script))
        return { "valid": false, "message": "脚本必须声明 request.url", "variables": [] };
    if (!/\bextractor\s*:/.test(script))
        return { "valid": false, "message": "脚本必须声明 extractor", "variables": [] };

    var variables = [];
    plans = Array.isArray(plans) ? plans : [];
    for (var i = 0; i < plans.length; ++i) {
        var references = [plans[i].usedVariable, plans[i].limitVariable];
        for (var j = 0; j < references.length; ++j) {
            var name = variableName(references[j]);
            if (!name)
                return { "valid": false, "message": "变量名必须使用 ${name} 格式", "variables": variables };
            if (variables.indexOf(name) < 0)
                variables.push(name);
            if (!_hasResultKey(script, name)) {
                return {
                    "valid": false,
                    "message": "extractor 返回值缺少变量 " + name,
                    "variables": variables
                };
            }
        }
        var resetReference = typeof plans[i].resetVariable === "string"
            ? plans[i].resetVariable.trim() : "";
        if (resetReference) {
            var resetName = variableName(resetReference);
            if (!resetName)
                return { "valid": false, "message": "变量名必须使用 ${name} 格式", "variables": variables };
            if (variables.indexOf(resetName) < 0)
                variables.push(resetName);
        }
    }
    return { "valid": true, "message": "脚本契约有效", "variables": variables };
}

function formatJavaScript(script) {
    var lines = String(script || "").replace(/\r\n?/g, "\n").split("\n");
    var depth = 0;
    var output = [];
    // ponytail: lightweight formatter for the documented object template; use a worker parser
    // if arbitrary JavaScript formatting becomes a hard requirement.
    for (var i = 0; i < lines.length; ++i) {
        var trimmed = lines[i].replace(/\s+$/g, "").trim();
        if (!trimmed && i === lines.length - 1)
            continue;
        if (/^[}\])]/.test(trimmed))
            depth = Math.max(0, depth - 1);
        output.push(new Array(depth * 4 + 1).join(" ") + trimmed);
        if (/[{[(]\s*,?$/.test(trimmed))
            ++depth;
    }
    return output.join("\n") + "\n";
}

function lineNumbers(text) {
    var count = Math.max(1, String(text || "").split("\n").length);
    var result = [];
    for (var i = 1; i <= count; ++i)
        result.push(String(i));
    return result.join("\n");
}
