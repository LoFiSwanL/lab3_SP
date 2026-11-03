# Runtime for Python code generated from the Mini-C AST.
from contextlib import contextmanager


class Runtime:
    def __init__(self):
        self.scopes = []
        self.steps = 0

    @contextmanager
    def block(self):
        self.scopes.append({})
        try:
            yield
        finally:
            self.scopes.pop()

    def tick(self, line):
        self.steps += 1
        if self.steps > 100000:
            self.error(line, "перевищено ліміт 100000 кроків")

    def error(self, line, message):
        raise RuntimeError(f"Рядок {line}: {message}")

    def integer(self, value, line):
        if not -2147483648 <= value <= 2147483647:
            self.error(line, "переповнення int")
        return value

    def declare(self, name, line):
        if name in self.scopes[-1]:
            self.error(line, "повторне оголошення змінної " + name)
        self.scopes[-1][name] = None

    def scope(self, name, line):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope
        self.error(line, "неоголошена змінна " + name)

    def get(self, name, line):
        value = self.scope(name, line)[name]
        if value is None:
            self.error(line, "неініціалізована змінна " + name)
        return value

    def set(self, name, value, line):
        self.scope(name, line)[name] = value

    def unary(self, op, value, line):
        if op == "!":
            return int(not value)
        return self.integer(-value if op == "-" else value, line)

    def binary(self, op, a, b, line):
        if op in ("/", "%"):
            if b == 0:
                self.error(line, "ділення на нуль")
            quotient = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                quotient = -quotient
            self.integer(quotient, line)
            return quotient if op == "/" else a - quotient * b
        if op == "+":
            return self.integer(a + b, line)
        if op == "-":
            return self.integer(a - b, line)
        if op == "*":
            return self.integer(a * b, line)
        return int({"<": a < b, ">": a > b, "<=": a <= b,
                    ">=": a >= b, "==": a == b, "!=": a != b}[op])


rt = Runtime()


def main():
    rt.tick(2)
    with rt.block():
        rt.tick(3)
        rt.declare('n', 3)
        rt.set('n', 5, 3)
        rt.tick(4)
        rt.declare('result', 4)
        rt.set('result', 1, 4)
        rt.tick(5)
        while rt.binary('>', rt.get('n', 5), 1, 5):
            rt.tick(5)
            with rt.block():
                rt.tick(6)
                rt.set('result', rt.binary('*', rt.get('result', 6), rt.get('n', 6), 6), 6)
                rt.tick(7)
                rt.set('n', rt.binary('-', rt.get('n', 7), 1, 7), 7)
        rt.tick(9)
        if rt.binary('==', rt.get('result', 9), 120, 9):
            rt.tick(9)
            with rt.block():
                rt.tick(10)
                return rt.get('result', 10)
        else:
            rt.tick(11)
            with rt.block():
                rt.tick(12)
                return 0
    return 0


if __name__ == '__main__':
    try:
        print(main())
    except RuntimeError as error:
        import sys
        print(error, file=sys.stderr)
        sys.exit(1)
