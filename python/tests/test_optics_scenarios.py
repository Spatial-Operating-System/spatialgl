import unittest

from examples.optics.scenarios import (
    composition_outcomes,
    f01_overlap_occlusion,
    f02_hybrid_and_unsupported_fill,
    f03_shared_steering_contention,
    f04_delayed_moving_plane,
    f05_async_scan_pair,
    f06_cancel_fault_and_revision,
)


class OpticsScenarioTest(unittest.TestCase):
    def test_all_surface_optics_acceptance_scenarios(self):
        for scenario in (
            f01_overlap_occlusion,
            f02_hybrid_and_unsupported_fill,
            f03_shared_steering_contention,
            f04_delayed_moving_plane,
            f05_async_scan_pair,
            f06_cancel_fault_and_revision,
            composition_outcomes,
        ):
            with self.subTest(scenario=scenario.__name__):
                self.assertTrue(scenario())


if __name__ == "__main__":
    unittest.main()
