"""Exercise build reporting failure paths without a toolchain or hardware."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import build_unified as builder
import package_sentinel as packager


class BuildReportChecks(unittest.TestCase):
    def run_build(self, exit_code, outputs):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / ".pio/build/example"
            build.mkdir(parents=True)
            for name in outputs:
                (build / name).write_bytes(b"test output")
            with patch.object(builder, "ROOT", root), \
                 patch.object(builder, "TARGETS", {"example": (root, "example")}), \
                 patch("sys.argv", ["build_unified.py"]), \
                 patch.object(builder.subprocess, "run", return_value=SimpleNamespace(returncode=exit_code)) as run, \
                 contextlib.redirect_stdout(io.StringIO()):
                code = builder.main()
                command = run.call_args.args[0]
                self.assertNotIn("upload", command)
                self.assertNotIn("erase", command)
            report = json.loads((root / "artifacts/integration/build-report.json").read_text())
            return code, report

    def test_failed_build_does_not_report_stale_binaries(self):
        code, report = self.run_build(1, ["firmware.bin", "firmware.elf", "partitions.bin"])
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "failed")
        self.assertNotIn("files", report["targets"]["example"])

    def test_success_without_required_images_is_failure(self):
        code, report = self.run_build(0, ["firmware.bin"])
        self.assertEqual(code, 1)
        self.assertEqual(report["targets"]["example"]["missing_outputs"], ["firmware.elf", "partitions.bin"])

    def test_complete_build_records_checksums_without_hardware_claim(self):
        code, report = self.run_build(0, ["firmware.bin", "firmware.elf", "partitions.bin"])
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "passed")
        self.assertFalse(report["hardware_validated"])
        self.assertEqual(len(report["targets"]["example"]["files"]["firmware.bin"]["sha256"]), 64)


class MatchedConsoleProofChecks(unittest.TestCase):
    """The standalone packager must reject stale or missing native evidence."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        patcher = patch.object(packager, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.bound = (".pio/build/s3-main/firmware.elf", "s3_main/main/usb_console.c", "tools/build_unified.py")
        for name in self.bound:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode("utf-8"))
        self.proof = {
            "status": "passed", "hardware_validated": False,
            "sha256": packager.sha256(self.root / self.bound[0]),
            "source_sha256": packager.sha256(self.root / self.bound[1]),
            "checker_sha256": packager.sha256(self.root / self.bound[2]),
            "allocated_stack_bytes": 4096, "task_frame_limit_bytes": 192,
            "frames_bytes": {"usb_console_task": 112}, "known_frame_bytes": 2848,
            "remaining_after_known_frames_bytes": 1248,
            "minimum_remaining_after_known_frames_bytes": 1024,
        }
        self.json_path = self.root / "artifacts/integration/s3-console-stack.json"
        self.json_path.parent.mkdir(parents=True, exist_ok=True)
        self.json_path.write_text(json.dumps(self.proof), encoding="utf-8")
        self.text_path = self.json_path.with_suffix(".txt")
        self.text_path.write_text("synthetic debugger evidence", encoding="utf-8")
        self.receipt = {"native_console_stack": self.proof,
            "native_console_evidence": {path.relative_to(self.root).as_posix(): packager.sha256(path)
                for path in (self.json_path, self.text_path)}}

    def test_matching_final_elf_and_evidence_are_accepted(self):
        self.assertEqual(packager.validate_console_proof(self.receipt), self.proof)

    def test_legacy_receipt_without_native_proof_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Missing native console proof"):
            packager.validate_console_proof({})

    def test_each_bound_input_change_rejects_the_old_proof(self):
        for name in self.bound:
            with self.subTest(path=name):
                path = self.root / name
                original = path.read_bytes()
                path.write_bytes(original + b" changed")
                with self.assertRaisesRegex(ValueError, "proof is stale"):
                    packager.validate_console_proof(self.receipt)
                path.write_bytes(original)

    def test_changed_debugger_evidence_is_rejected(self):
        self.text_path.write_text("different output", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "evidence changed"):
            packager.validate_console_proof(self.receipt)

    def test_invalid_native_budget_cannot_be_packaged(self):
        self.proof["known_frame_bytes"] = 4096
        self.proof["remaining_after_known_frames_bytes"] = 0
        with self.assertRaisesRegex(ValueError, "frame budget"):
            packager.validate_console_proof(self.receipt)


if __name__ == "__main__":
    unittest.main()
