// FEATURE ship graph: council scouts → implement → review → verify.
// Usage: /workflow ka-ship {"objective":"...","gis":true}

let meta = #{
    name: "ka-ship",
    description: "ka-hgis FEATURE graph: parallel scouts, implementer, reviewer, fail-closed tester",
    when_to_use: "Multi-file FEATURE that should keep moving through experts without stalling",
    phases: [
        #{ title: "Scout", detail: "app+core+tests (+gis)" },
        #{ title: "Implement", detail: "ka-implementer" },
        #{ title: "Review", detail: "ka-reviewer" },
        #{ title: "Verify", detail: "ka-tester" },
    ],
};

let brief_schema = #{
    "type": "object", "required": ["summary", "paths", "risks"],
    "properties": #{
        "summary": #{ "type": "string" },
        "paths": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 20 },
        "risks": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 8 },
    },
};
let impl_schema = #{
    "type": "object", "required": ["done", "changed_files", "verify_cmd"],
    "properties": #{
        "done": #{ "type": "boolean" },
        "changed_files": #{ "type": "array", "items": #{ "type": "string" } },
        "verify_cmd": #{ "type": "string" },
        "notes": #{ "type": "string" },
    },
};
let review_schema = #{
    "type": "object", "required": ["ok", "findings"],
    "properties": #{
        "ok": #{ "type": "boolean" },
        "findings": #{ "type": "array", "items": #{ "type": "string" }, "maxItems": 8 },
    },
};
let verify_schema = #{
    "type": "object", "required": ["pass", "evidence"],
    "properties": #{
        "pass": #{ "type": "boolean" },
        "evidence": #{ "type": "string" },
    },
};

let objective = if args == () { () } else { args.objective };
if (objective == ()) { pause("verification", "Pass args.objective — what to ship."); }

let want_gis = true;
if (args != () && args.gis == false) { want_gis = false; }

let inv = " MUST NOT: fork QGIS; removeAllMapLayers on load; empty-layer auto-add; hardcode VWorld key; export CRS != 5179; DXF submit; grow MainWindow when core can hold it.";

phase("Scout");
let jobs = [
    #{
        prompt: "TASK: " + objective + " Area: src/app. Read-only scout. Return summary, paths[], risks[]." + inv,
        label: "scout:app",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    },
    #{
        prompt: "TASK: " + objective + " Area: src/core. Read-only scout. Return summary, paths[], risks[]." + inv,
        label: "scout:core",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    },
    #{
        prompt: "TASK: " + objective + " Area: tests/ scripts/. Find cmake/ctest/smoke commands. Return summary, paths[], risks[]." + inv,
        label: "scout:tests",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    },
];
if (want_gis) {
    jobs.push(#{
        prompt: "TASK: " + objective + " You are qgis-api. Cookbook + LayerOps wiring. Return summary, paths[], risks[]." + inv,
        label: "qgis-api",
        agent_type: "qgis-api",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
}

let scouts = parallel(jobs);
let brief = "";
for r in scouts {
    if (r != () && r.success && r.output != () && r.output.summary != ()) {
        brief += r.output.summary;
        brief += "\n";
    }
}
if (brief == "") { brief = "(scouts empty; implement from objective only)\n"; }

phase("Implement");
let impl_prompt = "";
impl_prompt += "You are ka-implementer. TASK: ";
impl_prompt += objective;
impl_prompt += " EXPECTED OUTCOME: smallest correct C++/Qt change. CONTEXT:\n";
impl_prompt += brief;
impl_prompt += inv;
impl_prompt += " Return done, changed_files[], verify_cmd, notes.";

let impl = agent(impl_prompt, #{
    label: "ka-implementer",
    agent_type: "ka-implementer",
    capability_mode: "all",
    output_schema: impl_schema,
});
if (impl == () || !impl.success) {
    complete(#{ summary: "implementer failed", pass: false });
}

phase("Review");
let files = "";
if (impl.output != () && impl.output.changed_files != ()) {
    for f in impl.output.changed_files {
        files += f;
        files += " ";
    }
}
let rev = agent(
    "You are ka-reviewer. Read git diff and these files: " + files
        + " Objective: " + objective + " Flag bugs and invariant breaks only. Return ok, findings[]." + inv,
    #{
        label: "ka-reviewer",
        agent_type: "ka-reviewer",
        capability_mode: "read-only",
        output_schema: review_schema,
    },
);
if (rev == () || !rev.success || rev.output == () || rev.output.ok != true) {
    let findings = [];
    if (rev != () && rev.success && rev.output != () && rev.output.findings != ()) {
        findings = rev.output.findings;
    }
    complete(#{ summary: "review blocked ship", pass: false, findings: findings });
}

phase("Verify");
let vcmd = "cmake --build build --config Release ; ctest --test-dir build -C Release --output-on-failure";
if (impl.output != () && impl.output.verify_cmd != ()) { vcmd = impl.output.verify_cmd; }
let tester = agent(
    "You are ka-tester. Run: " + vcmd
        + " Use scripts/dev-env.ps1 and CMake on PATH (C:\\CMake\\bin or Program Files\\CMake\\bin)."
        + " pass=true only if every command exited 0. evidence = exit codes + first error.",
    #{
        label: "ka-tester",
        agent_type: "ka-tester",
        capability_mode: "execute",
        output_schema: verify_schema,
    },
);
if (tester == () || !tester.success || tester.output == () || tester.output.pass != true) {
    let ev = "";
    if (tester != () && tester.success && tester.output != () && tester.output.evidence != ()) {
        ev = tester.output.evidence;
    }
    complete(#{ summary: "verify failed", pass: false, evidence: ev });
}

complete(#{
    summary: "ka-ship passed",
    pass: true,
    evidence: tester.output.evidence,
    changed_files: impl.output.changed_files,
});
