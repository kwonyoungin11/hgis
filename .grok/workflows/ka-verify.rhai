// Graph: optional scout → verify evidence gate (cmake/ctest/smoke via ka-tester)
// Usage: /workflow ka-verify {"scope":"src/core/LayerOps.cpp"}

let meta = #{
    name: "ka-verify",
    description: "ka-hgis verify graph: scout touched C++ then fail-closed cmake/ctest",
    when_to_use: "After FEATURE C++ edits when you need an evidence-gated build/test loop",
    phases: [
        #{ title: "Scout", detail: "what changed and which gate applies" },
        #{ title: "Verify", detail: "ka-tester runs cmake/ctest" },
    ],
};

let scout_schema = #{
    "type": "object", "required": ["summary", "need_build", "need_smoke"],
    "properties": #{
        "summary": #{ "type": "string" },
        "need_build": #{ "type": "boolean" },
        "need_smoke": #{ "type": "boolean" },
    },
};
let verify_schema = #{
    "type": "object", "required": ["pass", "evidence"],
    "properties": #{
        "pass": #{ "type": "boolean" },
        "evidence": #{ "type": "string" },
    },
};

let scope = if args == () { () } else { args.scope };
if (scope == ()) { scope = "src tests CMakeLists.txt"; }

phase("Scout");
let scout = agent(
    "Read-only. Inspect git status and the scope: " + scope
        + ". Use grep/read_file/run_terminal_command (git status, git diff --stat) only."
        + " Decide need_build (C++/CMake/tests changed) and need_smoke (startup/menu/UI path)."
        + " Do not edit files.",
    #{ label: "scout:gates", capability_mode: "read-only", output_schema: scout_schema }
);

if (scout == () || !scout.success || scout.output == ()) {
    pause("verification", "Scout failed; resume after the tree is readable.");
}

if (scout.output.need_build != true && scout.output.need_smoke != true) {
    complete(#{ summary: "docs/rules only — no cmake gate", pass: true });
}

phase("Verify");
let smoke_bit = "";
if (scout.output.need_smoke == true) { smoke_bit = " Also run .\\scripts\\run-ka-hgis.ps1 --smoke-quit."; }
let vprompt = "";
vprompt += "You are ka-tester for ka-hgis on Windows. ";
vprompt += "MUST DO: $env:PATH = 'C:\\CMake\\bin;' + $env:PATH; . .\\scripts\\dev-env.ps1; ";
vprompt += "cmake --build build --config Release; ctest --test-dir build -C Release --output-on-failure.";
vprompt += smoke_bit;
vprompt += " MUST NOT: edit product source; delete tests; claim pass without this turn's output. ";
vprompt += " After pass, write an ISO timestamp to .grok/.state/last-verify. ";
vprompt += "Return pass=true only if every requested command exited 0. evidence = exit codes + first error.";

let verify = agent(vprompt, #{
    label: "ka-tester",
    agent_type: "ka-tester",
    capability_mode: "execute",
    output_schema: verify_schema,
});

if (verify == () || !verify.success || verify.output == ()) {
    complete(#{ summary: "verify agent failed", pass: false });
}
if (verify.output.pass != true) {
    complete(#{ summary: "verify failed", pass: false, evidence: verify.output.evidence });
}
complete(#{ summary: "verify passed", pass: true, evidence: verify.output.evidence });
