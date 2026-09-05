"""Build diagnostic and failure-propagation contracts of the shipping verifier."""

from __future__ import annotations

import pathlib
import subprocess
import unittest
from unittest import mock

from verify import verify_runtime


class RuntimeVerificationTests(unittest.TestCase):
    def test_successful_keep_going_build_precedes_ctest(self) -> None:
        build_dir = pathlib.Path("build/verify")
        with (
            mock.patch("verify.require_program", side_effect=lambda name: name),
            mock.patch("verify.run") as run,
        ):
            verify_runtime(build_dir, 3)

        self.assertEqual(
            run.call_args_list,
            [
                mock.call(("cmake", "--build", str(build_dir), "--parallel", "3", "--", "-k", "0")),
                mock.call(("ctest", "--test-dir", str(build_dir), "--output-on-failure")),
            ],
        )

    def test_build_failure_propagates_without_resolving_or_running_ctest(self) -> None:
        failure = subprocess.CalledProcessError(1, "cmake")
        with (
            mock.patch("verify.require_program", return_value="cmake") as require_program,
            mock.patch("verify.run", side_effect=failure) as run,
            self.assertRaises(subprocess.CalledProcessError) as raised,
        ):
            verify_runtime(pathlib.Path("build/verify"), 2)

        self.assertIs(raised.exception, failure)
        require_program.assert_called_once_with("cmake")
        run.assert_called_once()

    def test_invalid_parallelism_refuses_before_build(self) -> None:
        with (
            mock.patch("verify.require_program") as require_program,
            mock.patch("verify.run") as run,
            self.assertRaisesRegex(ValueError, "--parallel must be positive"),
        ):
            verify_runtime(pathlib.Path("build/verify"), 0)

        require_program.assert_not_called()
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
