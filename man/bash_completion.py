#!/usr/bin/env python3

# Extract options from markdown and generate bash completion.

import sys
import re
from pathlib import Path


def extract_options(markdown_file):
    # Extract command line options from markdown file.
    options = []
    with open(markdown_file, 'r', encoding='utf-8') as f:
        for line in f:
            match = re.match(r'^\*\*`(-[^`]+)`\*\*', line.strip())
            if match:
                # Strip arguments (everything after space)
                opt = match.group(1).split()[0]
                options.append(opt)
    return ' '.join(options)


def process_bash_completion(template_file, output_file, project_shortname, options):
    # Process bash completion template.
    with open(template_file, 'r', encoding='utf-8') as f:
        content = f.read()

    content = content.replace('@PROJECT_SHORTNAME@', project_shortname)
    content = content.replace('@content', options)

    Path(output_file).parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(content)
    return True


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('markdown_file')
    parser.add_argument('output_file')
    parser.add_argument('project_shortname')
    args = parser.parse_args()

    options = extract_options(args.markdown_file)
    template_file = Path(__file__).parent / 'bash-completion' / 'woof.template'
    process_bash_completion(template_file, args.output_file, args.project_shortname, options)
    return 0


if __name__ == '__main__':
    sys.exit(main())
