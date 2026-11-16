import csv
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from analyze import analyze, read_spans


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def run_fixture(self, scenario="nested", enabled=True, **environment):
        binary = Path(os.environ["PROFILE_TEST_BIN"]) / ("enabled" if enabled else "disabled")
        env = {**os.environ, "NIMF_PROFILE_DIR": str(self.directory), "NIMF_PROFILE_CPU": "1", **environment}
        return subprocess.run([str(binary), scenario], env=env, capture_output=True, text=True, check=True)

    def test_nested_early_return(self):
        self.run_fixture()
        spans = read_spans(self.directory)
        self.assertEqual(len(spans), 2)
        self.assertEqual(spans[0].children, [spans[1]])
        self.assertGreater(spans[1].wall, 0)
        self.assertGreater(spans[1].cpu, 0)
        self.assertGreater(spans[0].self_wall, 0)

    def test_thread_exit_and_main_exit_flush(self):
        self.run_fixture("threads")
        spans = read_spans(self.directory)
        self.assertEqual(len(spans), 162)
        self.assertEqual(len({(span.pid, span.thread) for span in spans}), 5)

    def test_compile_and_runtime_disabled(self):
        self.run_fixture(enabled=False)
        self.run_fixture(NIMF_PROFILE_DIR="")
        self.assertEqual(list(self.directory.iterdir()), [])

    def test_overflow_is_rejected(self):
        self.run_fixture("overflow", NIMF_PROFILE_CAPACITY="256")
        with self.assertRaisesRegex(ValueError, "incomplete trace"):
            read_spans(self.directory)

    def test_existing_file_is_preserved(self):
        result = self.run_fixture("collision")
        self.assertIn("cannot create trace", result.stderr)
        self.assertEqual(next(self.directory.iterdir()).read_text(), "preserved\n")

    def test_truncated_file_is_rejected(self):
        self.run_fixture()
        file = next(self.directory.iterdir())
        file.write_text("\n".join(file.read_text().splitlines()[:-1]) + "\n")
        with self.assertRaisesRegex(ValueError, "truncated trace"):
            read_spans(self.directory)

    def test_invalid_parent_is_rejected(self):
        self.run_fixture()
        file = next(self.directory.iterdir())
        lines = file.read_text().splitlines()
        lines[-1] = lines[-1].replace("2,1,", "2,99,", 1)
        file.write_text("\n".join(lines) + "\n")
        with self.assertRaisesRegex(ValueError, "invalid parent"):
            read_spans(self.directory)

    def test_analysis_pairs_windows_and_rejects_ambiguity(self):
        profile = self.directory / "profile"
        profile.mkdir()
        for pid, name, start, end in ((1, "client.filter", 110000, 190000),
                                      (2, "server.filter", 120000, 170000)):
            (profile / f"nimf-{pid}-1.csv").write_text(
                f"# nimf-profile-v1 pid={pid} thread=1 cpu=0 records=1 dropped=0 incomplete=0 clock_errors=0\n"
                "id,parent,span,detail,start_ns,end_ns,cpu_start_ns,cpu_end_ns\n"
                f"1,0,{name},0,{start},{end},0,0\n")
        (self.directory / "samples.csv").write_text(
            "phase,step,cycle,injected_us,screen_upper_us,model_ms\n"
            "measured,preedit,1,100,200,0.080\n")
        analyze(self.directory)
        keys = list(csv.DictReader(io.StringIO((self.directory / "profile-keys.csv").read_text())))
        self.assertEqual(float(keys[0]["client_us"]), 80)
        self.assertEqual(float(keys[0]["outside_server_us"]), 30)
        report = json.loads((self.directory / "profile-summary.json").read_text())
        self.assertIsNone(report[0]["cpu_p50_us"])
        for pid, name, start, end in ((1, "client.filter", 191000, 199000),
                                      (2, "server.filter", 192000, 198000)):
            file = profile / f"nimf-{pid}-1.csv"
            file.write_text(file.read_text().replace("records=1", "records=2") +
                            f"2,0,{name},0,{start},{end},0,0\n")
        analyze(self.directory)
        keys = list(csv.DictReader(io.StringIO((self.directory / "profile-keys.csv").read_text())))
        self.assertEqual(int(keys[0]["filter_calls"]), 2)
        self.assertEqual(float(keys[0]["client_us"]), 88)
        self.assertEqual(float(keys[0]["client_span_us"]), 89)
        client_file = profile / "nimf-1-1.csv"
        complete = client_file.read_text()
        for start, end in ((150000, 30000000), (90000, 105000)):
            client_file.write_text(complete.replace("records=2", "records=3") +
                                   f"3,0,client.filter,0,{start},{end},0,0\n")
            with self.assertRaisesRegex(ValueError, "crosses an input window"):
                analyze(self.directory)
        client_file.write_text(complete)
        (profile / "nimf-3-1.csv").write_text((profile / "nimf-1-1.csv").read_text().replace("pid=1", "pid=3"))
        with self.assertRaisesRegex(ValueError, "expected one client"):
            analyze(self.directory)


if __name__ == "__main__":
    unittest.main()
