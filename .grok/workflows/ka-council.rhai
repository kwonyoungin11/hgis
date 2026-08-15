// Multi-graph council: parallel experts, then a structured brief.
// Usage: /workflow ka-council {"task":"...","kind":"gis"|"feature"|"debug"}

let meta = #{
    name: "ka-council",
    description: "Summon a parallel ka-hgis expert council and return a structured brief",
    when_to_use: "전문가소환, expert council, unknown GIS or FEATURE scope before implement",
    phases: [
        #{ title: "Summon", detail: "parallel experts" },
        #{ title: "Brief", detail: "synthesize paths and risks" },
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

let task = if args == () { () } else { args.task };
if (task == ()) { pause("verification", "Pass args.task — what the council should inspect."); }

let kind = "feature";
if (args != () && args.kind != ()) { kind = args.kind; }

fn invariants() {
    " MUST NOT: fork QGIS; removeAllMapLayers on load; empty-layer auto-add; hardcode VWorld key; change export CRS from 5179; DXF submit. "
}

fn contract(task, extra) {
    let p = "";
    p += "TASK: ";
    p += task;
    p += " EXPECTED OUTCOME: existing file paths, risks, 5-8 line summary. Use read_file/grep/list_dir. Do not edit.";
    p += extra;
    p += invariants();
    p += " CONTEXT: ka-hgis C++20/Qt6, work CRS 5186/5187, export 5179, LayerOps::ensureDomainLayer only.";
    p
}

phase("Summon");
let jobs = [];
if (kind == "gis") {
    jobs.push(#{
        prompt: contract(task, " You are qgis-api. Cookbook + LayerOps/MainWindow wiring."),
        label: "qgis-api",
        agent_type: "qgis-api",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are gis-protocol. Published VWorld WMS/WMTS only."),
        label: "gis-protocol",
        agent_type: "gis-protocol",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are field-check. User-visible pass/fail only."),
        label: "field-check",
        agent_type: "field-check",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
} else if (kind == "debug") {
    jobs.push(#{
        prompt: contract(task, " You are ka-debugger. Competing hypotheses then evidence. Do not patch unless asked."),
        label: "ka-debugger",
        agent_type: "ka-debugger",
        capability_mode: "execute",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are ka-scout in tests/scripts. Find the proving command."),
        label: "ka-scout-tests",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are qgis-api if this could be CRS/layer/edit-buffer; else scout src/core."),
        label: "qgis-api",
        agent_type: "qgis-api",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
} else {
    jobs.push(#{
        prompt: contract(task, " You are ka-scout for src/app (MainWindow, map tools)."),
        label: "scout:app",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are ka-scout for src/core (LayerOps, export, checklist)."),
        label: "scout:core",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
    jobs.push(#{
        prompt: contract(task, " You are ka-scout for tests/ and scripts/ verify commands."),
        label: "scout:tests",
        agent_type: "ka-scout",
        capability_mode: "read-only",
        output_schema: brief_schema,
    });
}

let results = parallel(jobs);

phase("Brief");
let paths = [];
let risks = [];
let summaries = [];
for r in results {
    if (r != () && r.success && r.output != ()) {
        if (r.output.summary != ()) { summaries.push(r.output.summary); }
        if (r.output.paths != ()) {
            for p in r.output.paths { paths.push(p); }
        }
        if (r.output.risks != ()) {
            for k in r.output.risks { risks.push(k); }
        }
    }
}

let summary = "";
for s in summaries {
    summary += s;
    summary += "\n";
}
if (summary == "") { summary = "Council returned no usable brief."; }

log(paths.len().to_string() + " paths, " + risks.len().to_string() + " risks");
complete(#{
    summary: summary,
    paths: paths,
    risks: risks,
    kind: kind,
    next: "Spawn ka-implementer with this brief, then ka-reviewer and ka-tester.",
});
