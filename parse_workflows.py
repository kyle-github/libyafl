import os, glob, yaml

workflows = glob.glob('.github/workflows/*.yml')
workflows = [w for w in workflows if 'coverage' not in w and 'update-version' not in w and 'release' not in w]

# We will collect the matrix entries
print("Parsing workflows...")
