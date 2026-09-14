"""Validate full RGB geometry, hidden-axis penalties, ties and exact intervals."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import palette_gradient as tool


class Geometry(unittest.TestCase):
    def test_off_plane_penalty_changes_boundary(self):
        # Projected sites at R=0,100 have their 3D bisector at R=62.5, not 50,
        # because the second site is 50 units away from the G=100 plane.
        palette = np.array([[0., 100., 20.], [100., 50., 20.]])
        profile = np.array([[0., 100., 20.], [100., 100., 20.]])
        plane = tool.plane_for(profile, ("G", 100))
        cells = tool.slice_cells(palette, plane, [0, 100, 0, 40])
        self.assertEqual(len(cells), 2)
        self.assertAlmostEqual(cells[0]["vertices"][:, 0].max(), 62.5)
        self.assertAlmostEqual(sum(c["area"] for c in cells), 4000.)
        self.assertEqual(tool.nearest(np.array([[60., 100., 20.]]), palette)[0], 0)

    def test_chord_intervals_match_independent_distances(self):
        palette = np.array([[158., 255., 170.], [188., 234., 200.],
                            [216., 236., 216.], [214., 246., 232.]])
        start, end = np.array([159., 252., 172.]), np.array([217., 252., 228.])
        intervals = tool.segment_intervals(palette, start, end)
        self.assertAlmostEqual(intervals[1]["start"], .28289473684210525)
        self.assertAlmostEqual(intervals[1]["end"], .6928571428571428)
        t = (np.arange(10000) + .5) / 10000
        actual = tool.nearest(start + t[:, None] * (end - start), palette)
        expected = np.full(len(t), -1)
        for row in intervals:
            expected[(t >= row["start"]) & (t < row["end"])] = row["index"]
        np.testing.assert_array_equal(actual, expected)

    def test_plane_ties_do_not_double_count_cells(self):
        palette = np.array([[30., 95., 40.], [30., 105., 40.], [30., 95., 40.]])
        plane = tool.plane_for(np.array([[10., 100., 20.], [80., 100., 80.]]), ("G", 100))
        cells = tool.slice_cells(palette, plane, [0, 100, 0, 100])
        self.assertEqual([c["index"] for c in cells], [0])
        self.assertAlmostEqual(cells[0]["area"], 10000.)

    def test_chord_on_bisector_uses_first_site(self):
        palette = np.array([[30., 95., 40.], [30., 105., 40.], [30., 95., 40.]])
        start, end = np.array([0., 100., 40.]), np.array([100., 100., 40.])
        self.assertEqual(tool.segment_intervals(palette, start, end),
                         [{"index": 0, "start": 0., "end": 1.}])
        self.assertTrue((tool.nearest(np.linspace(start, end, 100), palette) == 0).all())

    def test_fitted_plane_contains_the_chord_and_clips_rgb_cube(self):
        profile = np.linspace([0., 0., 0.], [255., 255., 255.], 64)
        plane = tool.plane_for(profile)
        self.assertLess(plane["max_off_plane"], 1e-10)
        cells = tool.slice_cells(profile[::16], plane, [-400, 400, -200, 200])
        for cell in cells:
            vertices = plane["origin"] + cell["vertices"] @ plane["basis"]
            self.assertTrue((vertices >= -1e-7).all())
            self.assertTrue((vertices <= 255 + 1e-7).all())


if __name__ == "__main__":
    unittest.main()
