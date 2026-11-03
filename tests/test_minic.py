"""End-to-end checks: Flex/Bison -> AST -> interpreter / Python / native C."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/minic"


class MiniCTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def parse(self, source, *options):
        file = self.path / "input.c"
        file.write_text(source, encoding="utf-8")
        return subprocess.run([str(BINARY), str(file), *map(str, options)],
                              capture_output=True, text=True, timeout=5)

    def assert_result(self, source, expected):
        target = self.path / "output.py"
        result = self.parse(source, "--run", "--python", target)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f"Результат main: {expected}\n", result.stdout)
        translated = subprocess.run([sys.executable, str(target)],
                                    capture_output=True, text=True, timeout=5)
        self.assertEqual(translated.returncode, 0, translated.stderr)
        self.assertEqual(translated.stdout.strip(), str(expected))
        # Compare against the real C compiler when present (same input).
        if shutil.which("cc"):
            reference = self.path / "reference.c"
            reference_source = source.rsplit("}", 1)[0] + "return 0;}"
            reference.write_text("#define main source_main\n" + reference_source +
                                 '\n#undef main\n#include <stdio.h>\n'
                                 'int main(void) { printf("%d\\n", source_main()); return 0; }\n')
            native = self.path / "reference"
            compiled = subprocess.run(["cc", "-std=c11", str(reference), "-o", str(native)],
                                      capture_output=True, text=True, timeout=10)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            ran = subprocess.run([str(native)], capture_output=True, text=True, timeout=5)
            self.assertEqual(ran.stdout.strip(), str(expected))

    def test_valid_programs(self):
        cases = [
            ((ROOT / "examples/factorial.c").read_text(), 120),
            ((ROOT / "examples/scope.c").read_text(), 10),
            ("int main(){ return 2 + 3 * 4; }", 14),
            ("int main(){ return (2 + 3) * 4; }", 20),
            ("int main(){ return -7 / 3 * 10 + -7 % 3; }", -21),
            ("int main(){ return 0x10 + 010 + 10; }", 34),
            ("int main(){ return !0 + +3 + (2 <= 2) + (4 >= 5) + (1 != 2); }", 6),
            ("int main(){ return 1 || 0 && 0; }", 1),
            ("int main(){ return 0 && 10 / 0; }", 0),
            ("int main(){ return 1 || 10 / 0; }", 1),
            ("int main(){ if(1) if(0) return 1; else return 2; return 3; }", 2),
            ("int main(){ int n=3; while(n>0){ if(n==2) return n; n=n-1; } return 9; }", 2),
            ("int main(){ int class=7; int True; True=class+1; return True; }", 8),
            ("int main(){ /* a\nb */ int x; x=4; // test\n return x; }", 4),
            ("int main(){}", 0),
        ]
        for source, expected in cases:
            with self.subTest(source=source):
                self.assert_result(source, expected)

    def test_syntax_and_lexical_errors(self):
        cases = [
            ("int main(){\n int x=5\n return x; }", "Рядок 3"),
            ("int main(){ return (2+3; }", "syntax error"),
            ("int main(){ return 1;", "syntax error"),
            ("int main(){ return @; }", "невідомий символ"),
            ("int main(){ /* comment", "незакритий коментар"),
            ("int main(){ return 09; }", "літерал"),
            ("int main(){ return 2abc; }", "літерал"),
            ("int main(){ return 1.5; }", "літерал"),
            ("int main(){ if(1) int x=1; return 0; }", "syntax error"),
            ("int main(){ int for=1; return for; }", "ключове слово"),
            ("int main(){ return --1; }", "оператор C"),
        ]
        for source, message in cases:
            with self.subTest(source=source):
                result = self.parse(source)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)

    def test_runtime_errors_match_python(self):
        cases = [
            ("int main(){ return 1/0; }", "ділення на нуль"),
            ("int main(){ return x; }", "неоголошена змінна"),
            ("int main(){ int x; return x; }", "неініціалізована змінна"),
            ("int main(){ int x=1; {int x=x;} return 0; }", "неініціалізована змінна"),
            ("int main(){ int x=1; int x=2; return x; }", "повторне оголошення"),
            ("int main(){ {int x=1;} return x; }", "неоголошена змінна"),
            ("int main(){ return 2147483647+1; }", "переповнення int"),
            ("int main(){ return (-2147483647-1)/-1; }", "переповнення int"),
            ("int main(){ while(1); }", "100000 кроків"),
        ]
        for source, message in cases:
            with self.subTest(source=source):
                result = self.parse(source, "--run")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                target = self.path / "output.py"
                generated = self.parse(source, "--python", target)
                self.assertEqual(generated.returncode, 0, generated.stderr)
                translated = subprocess.run([sys.executable, str(target)],
                                            capture_output=True, text=True, timeout=5)
                self.assertNotEqual(translated.returncode, 0)
                self.assertEqual(translated.stderr, result.stderr)

    def test_literal_range(self):
        for literal in ("2147483648", "99999999999999999999999999"):
            result = self.parse(f"int main(){{ return {literal}; }}", "--run")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("літерал поза діапазоном", result.stderr)

    def test_html_contains_real_ast(self):
        target = self.path / "ast.html"
        result = self.parse("int main(){ return 2+3*4; }", "--html", target)
        self.assertEqual(result.returncode, 0, result.stderr)
        html = target.read_text()
        match = re.search(r'<script id="ast-data" type="application/json">(.*?)</script>', html)
        tree = json.loads(match.group(1))
        expression = tree["children"][0]["children"][0]["children"][0]
        self.assertEqual(expression["value"], "+")
        self.assertEqual(expression["children"][1]["value"], "*")
        self.assertNotIn("https://", html)  # Viewer works offline.

    def test_cli_errors(self):
        for options in [("--unknown",), ("--python",)]:
            result = self.parse("int main(){}", *options)
            self.assertNotEqual(result.returncode, 0)
        result = self.parse("int main(){}", "--python", self.path / "input.c")
        self.assertNotEqual(result.returncode, 0)

    def test_no_unresolved_grammar_conflicts(self):
        report = (ROOT / "build/parser.output").read_text()
        self.assertNotRegex(report.lower(), r"conflicts?:?\s+\d+\s+(shift/reduce|reduce/reduce)")


if __name__ == "__main__":
    unittest.main()
