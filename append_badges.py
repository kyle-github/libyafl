import yaml

with open('.github/workflows/ci.yml', 'r') as f:
    data = yaml.safe_load(f)

jobs = data.get('jobs', {})
job_names = [k for k in jobs.keys() if k != 'generate-badges']

steps = [
    {"uses": "actions/checkout@v4", "with": {"ref": "badges"}},
    {"name": "Set up directory", "run": "mkdir -p status"}
]

for j in job_names:
    run_script = f"""if [ "${{{{ needs.{j}.result }}}}" == "success" ]; then
  COLOR="brightgreen"
  TEXT="passing"
else
  COLOR="red"
  TEXT="failing"
fi
cat > status/{j}.svg << 'SVGEOF'
<svg xmlns="http://www.w3.org/2000/svg" width="220" height="20">
  <linearGradient id="b" x2="0" y2="100%">
    <stop offset="0" stop-color="#bbb" stop-opacity=".1"/>
    <stop offset="1" stop-opacity=".1"/>
  </linearGradient>
  <mask id="a">
    <rect width="220" height="20" rx="3" fill="#fff"/>
  </mask>
  <g mask="url(#a)">
    <rect width="160" height="20" fill="#555"/>
    <rect x="160" width="60" height="20" fill="${{COLOR}}"/>
    <rect width="220" height="20" fill="url(#b)"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,Geneva,sans-serif" font-size="11">
    <text x="80" y="14">{j}</text>
    <text x="190" y="14">${{TEXT}}</text>
  </g>
</svg>
SVGEOF
"""
    steps.append({
        "name": f"Generate badge for {j}",
        "run": run_script
    })

steps.append({
    "name": "Commit and push badges",
    "run": 'git config user.name "GitHub Actions"\ngit config user.email "actions@github.com"\ngit add status/*.svg\ngit diff --quiet && git diff --staged --quiet || (git commit -m "Update build status badges [skip ci]" && git push origin badges)'
})

badge_job = {
    'needs': list(job_names),
    'if': 'always()',
    'runs-on': 'ubuntu-latest',
    'steps': steps
}

data['jobs']['generate-badges'] = badge_job

class MyDumper(yaml.Dumper):
    def increase_indent(self, flow=False, indentless=False):
        return super(MyDumper, self).increase_indent(flow, False)

with open('.github/workflows/ci.yml', 'w') as f:
    yaml.dump(data, f, Dumper=MyDumper, sort_keys=False, default_flow_style=False)
