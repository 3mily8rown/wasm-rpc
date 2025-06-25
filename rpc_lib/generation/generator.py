import re
import sys
from pathlib import Path

STRUCT_RE = re.compile(r'typedef\s+struct\s+_(\w+)\s*{(.*?)}\s*\w+\s*;', re.DOTALL)
FIELD_RE = re.compile(r'\s*(\w+(?:_t)?)\s+(\w+)(?:\[(\d+)\])?;')
TAG_RE = re.compile(r'#define RpcEnvelope_(\w+)_tag\s+(\d+)')

def parse_structs(header_text):
    structs = {}
    for match in STRUCT_RE.finditer(header_text):
        struct_name = match.group(1)
        body = match.group(2)
        fields = []
        for field_match in FIELD_RE.finditer(body):
            field_type, field_name, array_size = field_match.groups()
            fields.append((field_type, field_name, array_size))
        structs[struct_name] = fields
    return structs

def parse_tags(header_text):
    return dict(TAG_RE.findall(header_text))

def cpp_type(field_type):
    return {
        'int32_t': 'int32_t',
        'uint32_t': 'uint32_t',
        'float': 'float',
        'bool': 'bool',
        'char': 'const char*',
        'pb_size_t': 'size_t'
    }.get(field_type, field_type)

def get_method_signature(fields):
    args = []
    for t, name, array in fields:
        if t == 'char':
            args.append(f'const char* {name}')
        elif array:
            args.append(f'const {t}* {name}, size_t count')
        else:
            args.append(f'{cpp_type(t)} {name}')
    return ', '.join(args)

def generate_stub(req_name, req_fields, resp_name, tags):
    method = req_name[0].lower() + req_name[1:]
    return_type = 'void'
    return_stmt = ''
    fail_stmt = 'return;'

    if 'info' in [f[1] for f in req_fields]:
        return_type = 'std::string'
        return_stmt = 'return std::string(resp.info);'
        fail_stmt = 'return {};'
    elif 'sum' in [f[1] for f in req_fields]:
        return_type = 'int32_t' if any(f[0] == 'int32_t' for f in req_fields) else 'float'
        return_stmt = 'return resp.sum;'
        fail_stmt = 'return -1;'
    else:
        return_type = 'bool'
        return_stmt = 'return true;'
        fail_stmt = 'return false;'

    lines = [f"{return_type} RpcClient::{method}({get_method_signature(req_fields)}) {{",
             f"    {req_name} msg = {req_name}_init_zero;"]

    for t, name, array in req_fields:
        if t == 'char':
            lines.append(f'    std::strncpy(msg.{name}, {name}, sizeof(msg.{name}) - 1);')
        elif array:
            lines.append(f'    if (count > PB_ARRAY_SIZE(&msg, {name})) {{')
            lines.append(f'        std::fprintf(stderr, "Too many items for {name}\\n");')
            lines.append(f'        {fail_stmt}')
            lines.append('    }')
            lines.append(f'    msg.{name}_count = count;')
            lines.append(f'    std::memcpy(msg.{name}, {name}, count * sizeof({t}));')
        else:
            lines.append(f'    msg.{name} = {name};')

    tag = tags.get(method, '/* unknown_tag */')
    lines.append(f'    if (!send(next_request_id_++, RpcEnvelope_{method}_tag, &msg)) {fail_stmt}')

    lines.append(f'    {resp_name} resp = {resp_name}_init_zero;')
    lines.append(f'    if (!receive<{resp_name}>(RpcResponse_{method}_tag, &resp)) {fail_stmt}')

    lines.append(f'    {return_stmt}')
    lines.append('}')

    return '\n'.join(lines)

def main(proto_path, header_path):
    header_text = Path(header_path).read_text()
    structs = parse_structs(header_text)
    tags = parse_tags(header_text)

    stubs = []

    for name in structs:
        if name.endswith("Response") or name in ("RpcEnvelope", "RpcResponse"):
            continue
        resp_name = name + "Response"
        if resp_name in structs:
            stub = generate_stub(name, structs[name], resp_name, tags)
            stubs.append(stub)

    if not stubs:
        print("No valid stub pairs found — check your header formatting.")
    else:
        Path("generated_stubs.cpp").write_text("\n\n".join(stubs))
        print("Generated stubs written to generated_stubs.cpp")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 generator.py <proto_file> <nanopb_header_file>")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
