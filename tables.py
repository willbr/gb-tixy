from math import *

# sqrt
# sin
# cos
# tan
# atan
# power

def chunk(l, n):
    for i in range(0, len(l), n):
        yield l[i:i + n]

def array_body(data):
    return ',\n    '.join(data)

def create_table(name, size, data):
    lines = ((', '.join(f"0x{x:02x}" for x in line))
        for line in chunk(table, 8))

    return f"""
unsigned char {name}[{size}] = {{
    {array_body(lines)}
}};
""".strip()

table = [int((sin(x * (pi / 127.5)) * 127.5) + 127.5) for x in range(256)]

print(create_table("sin_table", 256, table))

print("")

table = [int(sqrt(x)) for x in range(256)]

print(create_table("power_table", 256, table))
