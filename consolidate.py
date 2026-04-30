import glob, yaml, os

def get_workflows():
    workflows = glob.glob('.github/workflows/*.yml')
    ignores = ['coverage.yml', 'release.yml', 'update-version-readme.yml', 'ci.yml']
    return [w for w in workflows if os.path.basename(w) not in ignores]

def process_workflow(filepath):
    with open(filepath, 'r') as f:
        data = yaml.safe_load(f)
    
    target_name = data.get('name')
    jobs = data.get('jobs', {})
    
    combined_steps = []
    ordered_job_keys = []
    for prefix in ['setup', 'build', 'test']:
        for k in list(jobs.keys()):
            if k.startswith(prefix) and k not in ordered_job_keys:
                ordered_job_keys.append(k)
    for k in list(jobs.keys()):
        if k not in ordered_job_keys:
            ordered_job_keys.append(k)
            
    runs_on = None
    
    for k in ordered_job_keys:
        job = jobs[k]
        if not runs_on:
            runs_on = job.get('runs-on')
            
        steps = job.get('steps', [])
        for step in steps:
            if 'uses' in step:
                uses = step['uses']
                if 'actions/upload-artifact' in uses: continue
                if 'actions/download-artifact' in uses: continue
                if 'actions/checkout' in uses:
                    if any('actions/checkout' in s.get('uses', '') for s in combined_steps):
                        continue
            combined_steps.append(step)
            
    combined_job = {
        'runs-on': runs_on,
        'steps': combined_steps
    }
    
    for k in ordered_job_keys:
        for extra_key in ['container', 'services', 'env']:
            if extra_key in jobs[k] and extra_key not in combined_job:
                combined_job[extra_key] = jobs[k][extra_key]
                
    return target_name, combined_job

all_combined_jobs = {}
workflows_to_delete = get_workflows()

for w in workflows_to_delete:
    tname, cjob = process_workflow(w)
    # the job_key should be a valid github actions job ID
    job_key = os.path.basename(w).replace('.yml', '')
    all_combined_jobs[job_key] = cjob

ci_data = {
    'name': 'CI',
    'on': {
        'push': None,
        'pull_request': None
    },
    'jobs': all_combined_jobs
}

class MyDumper(yaml.Dumper):
    def increase_indent(self, flow=False, indentless=False):
        return super(MyDumper, self).increase_indent(flow, False)

with open('.github/workflows/ci.yml', 'w') as f:
    yaml.dump(ci_data, f, Dumper=MyDumper, sort_keys=False, default_flow_style=False)

print("Wrote ci.yml. Processed", len(workflows_to_delete), "workflows.")
