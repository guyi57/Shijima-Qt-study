#!/usr/bin/env python3
import sys, os

if len(sys.argv) < 2:
    print("Usage: bundle-default.py <files...>", file=sys.stderr)
    sys.exit(1)

print('#include "DefaultMascot.hpp"\n')
print('const std::map<std::string, std::pair<const char *, size_t>> defaultMascot = {')

for filepath in sys.argv[1:]:
    if not os.path.exists(filepath):
        continue
    with open(filepath, 'rb') as f:
        data = f.read()
    length = len(data)
    name = os.path.basename(filepath)
    data += b'\x00'
    hex_lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        escaped = ''.join(fr'\x{b:02X}' for b in chunk)
        hex_lines.append(f'    "{escaped}"')
    
    body = '\n'.join(hex_lines)
    print(f'\t{{ "{name}", {{\n{body}\n\t, {length} }} }},')

print('};')
