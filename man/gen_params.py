#!/usr/bin/env python3

import json
import sys
import argparse
from pathlib import Path


def load_json(json_path):
    with open(json_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def escape_c_string(s):
    # Escape string for C string literal.
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')


def generate_table_header(data, output_path):
    # Generate m_params_table.h with just the static table and section names.
    sections = {s['name']: s['display_name'] for s in data['sections']}

    lines = [
        "// AUTO-GENERATED FROM params.json - DO NOT EDIT MANUALLY",
        "// Run: python3 man/gen_params.py table",
        "",
        "#ifndef M_PARAMS_TABLE_H",
        "#define M_PARAMS_TABLE_H",
        "",
        "#include \"m_params.h\"",
        "",
        "static const param_def_t param_table[] = {",
    ]

    for param in data['parameters']:
        help_escaped = escape_c_string(param['help'])
        lines.append(f'    {{ "{param["name"]}", {param["type"]}, {param["section"]},')
        lines.append(f'      "{help_escaped}" }},')

    lines.append("};")
    lines.append("")

    lines.append("static const char *section_names[] = {")
    for section in data['sections']:
        lines.append(f'    [{section["name"]}] = "{section["display_name"]}",')
    lines.append("};")
    lines.append("")

    lines.append("#define PARAM_TABLE_SIZE (sizeof(param_table) / sizeof(param_table[0]))")
    lines.append("")
    lines.append("#endif // M_PARAMS_TABLE_H")

    output_path.write_text("\n".join(lines), encoding='utf-8')


def generate_markdown(data, output_path, project_name, project_shortname):
    # Generate Markdown documentation from JSON.
    sections = {s['name']: s['display_name'] for s in data['sections']}

    lines = [
        f"# {project_name}\n",
        f"{project_name} is a continuation of Lee Killough's Doom source port MBF targeted at modern systems\n",
        "## SYNOPSIS\n",
        f"`{project_shortname}` [*options*]\n",
        "## OPTIONS\n",
    ]

    current_section = None
    for param in data['parameters']:
        if param['section'] != current_section:
            current_section = param['section']
            lines.append(f"### {sections[current_section]}\n")

        arg_map = {
            'TypeString': ' <arg>',
            'TypeInt': ' <arg>',
            'TypeFloat': ' <arg>',
            'TypeFileList': ' <files>...',
            'TypeTwoArgs': ' <arg1> <arg2>',
            'TypeOneOrTwoArgs': ' <arg1> [<arg2>]',
            'TypeOptArg': ' [<arg>]',
            'TypeFlag': '',
        }
        arg = arg_map.get(param['type'], '')

        opt_sig = f"`{param['name']}{arg}`"
        lines.append(f"**{opt_sig}**")
        lines.append(f":   {param['help']}\n")

    output_path.write_text("\n".join(lines), encoding='utf-8')


def generate_bash_completion(data, output_path, project_shortname, template_path):
    # Generate bash completion from JSON using template.
    options = []
    for param in data['parameters']:
        opt = param['name'].split()[0]
        options.append(opt)

    options_str = ' '.join(options)

    with open(template_path, 'r', encoding='utf-8') as f:
        template = f.read()

    content = template.replace('@PROJECT_SHORTNAME@', project_shortname).replace('@content', options_str)
    output_path.write_text(content, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description='Generate parameter artifacts from params.json')
    subparsers = parser.add_subparsers(dest='command', required=True)

    # table command
    table_parser = subparsers.add_parser('table', help='Generate m_params_table.h')
    table_parser.add_argument('output', type=Path, help='Output header file')

    # markdown command
    md_parser = subparsers.add_parser('markdown', help='Generate Markdown documentation')
    md_parser.add_argument('output', type=Path, help='Output markdown file')
    md_parser.add_argument('project_name', help='Project display name')
    md_parser.add_argument('project_shortname', help='Project short name')

    # bash-completion command
    bc_parser = subparsers.add_parser('bash-completion', help='Generate bash completion')
    bc_parser.add_argument('output', type=Path, help='Output completion file')
    bc_parser.add_argument('project_shortname', help='Project short name')

    args = parser.parse_args()

    json_path = Path(__file__).parent / "params.json"
    data = load_json(json_path)

    if args.command == 'table':
        generate_table_header(data, args.output)
        print(f"Generated {args.output}")
    elif args.command == 'markdown':
        generate_markdown(data, args.output, args.project_name, args.project_shortname)
        print(f"Generated {args.output}")
    elif args.command == 'bash-completion':
        template_path = Path(__file__).parent / "bash-completion" / "woof.template"
        generate_bash_completion(data, args.output, args.project_shortname, template_path)
        print(f"Generated {args.output}")


if __name__ == "__main__":
    sys.exit(main())
