#!/usr/bin/env python3
"""Patch simdjson.h for GCC 16 C++26 module compatibility.

GCC 16 emits a hard error when anonymous-namespace types are exposed
through templates in a global module fragment. This script moves
escape_sequence from the anonymous namespace to a named one.
"""
import sys


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.h> <output.h>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        lines = f.readlines()

    patched = False
    out = []
    i = 0
    while i < len(lines):
        if (not patched and
                lines[i].strip() == 'struct escape_sequence {'):
            # Collect the struct body until closing };
            struct_lines = [lines[i]]
            j = i + 1
            while j < len(lines) and not lines[j].strip().startswith('};'):
                struct_lines.append(lines[j])
                j += 1
            struct_lines.append(lines[j])  # the };

            out.append('} // close anon namespace for GCC 16 module compat\n')
            out.append('namespace simdjson_compat_ {\n')
            for sl in struct_lines:
                out.append(sl)
            out.append('} // namespace simdjson_compat_\n')
            out.append('namespace {\n')
            out.append('using simdjson_compat_::escape_sequence;\n')

            i = j + 1
            patched = True
        else:
            out.append(lines[i])
            i += 1

    if not patched:
        print("WARNING: patch pattern not found in simdjson.h", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[2], 'w') as f:
        f.writelines(out)


if __name__ == '__main__':
    main()
