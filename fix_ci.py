import yaml

with open('.github/workflows/ci.yml', 'r') as f:
    data = yaml.safe_load(f)

for job_name, job_info in data['jobs'].items():
    if job_name == 'generate-badges': continue
    
    seen_step_names = set()
    new_steps = []
    
    for step in job_info.get('steps', []):
        if 'name' in step:
            name = step['name']
            
            # Skip redundant executable permission restore
            if name == 'Restore execute permissions': continue
            if step.get('run') and 'chmod +x build/bin' in str(step.get('run', '')): continue
                
            # Skip duplicated setup/install steps
            if name in seen_step_names: continue
                
            seen_step_names.add(name)
            
        new_steps.append(step)
        
    job_info['steps'] = new_steps

class MyDumper(yaml.Dumper):
    def increase_indent(self, flow=False, indentless=False):
        return super(MyDumper, self).increase_indent(flow, False)

with open('.github/workflows/ci.yml', 'w') as f:
    yaml.dump(data, f, Dumper=MyDumper, sort_keys=False, default_flow_style=False)
