import argparse

import cv2
import numpy as np


def load_outline(path):
    gray = cv2.imdecode(np.fromfile(path, np.uint8), cv2.IMREAD_GRAYSCALE)
    if gray is None:
        raise SystemExit(f"Couldn't read {path}")
    gray = cv2.copyMakeBorder(gray, 2, 2, 2, 2, cv2.BORDER_CONSTANT, value=0)
    track = (gray > 127).astype(np.uint8)

    _, labels = cv2.connectedComponents(1 - track, connectivity=4)
    shape = (labels != labels[0, 0]).astype(np.uint8)

    contours, _ = cv2.findContours(shape, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    if not contours:
        raise SystemExit("No track found in the image")
    outline = max(contours, key=cv2.contourArea)[:, 0, :].astype(float)

    x, y = outline[:, 0], outline[:, 1]
    if np.sum(x * np.roll(y, -1) - np.roll(x, -1) * y) < 0:
        outline = outline[::-1]
    return outline


def resample(points, step=1.0, sigma=3.0):
    closed = np.vstack([points, points[:1]])
    dist = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(closed, axis=0), axis=1))])
    t = np.arange(0, dist[-1], step)
    points = np.column_stack([np.interp(t, dist, closed[:, 0]), np.interp(t, dist, closed[:, 1])])

    r = int(3 * sigma)
    kernel = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    padded = np.vstack([points[-r:], points, points[:r]])
    return np.column_stack([np.convolve(padded[:, i], kernel, mode="valid") for i in (0, 1)])


def fit_to_window(points, size, margin):
    lo, hi = points.min(0), points.max(0)
    scale = (size - 2 * margin) / (hi - lo).max()
    offset = (size - (hi - lo) * scale) / 2 - lo * scale
    return points * scale + offset


def simplify(path, tol):
    keep = {0, len(path) - 1}
    stack = [(0, len(path) - 1)]
    while stack:
        a, b = stack.pop()
        if b - a < 2:
            continue
        chord = path[b] - path[a]
        rel = path[a + 1:b] - path[a]
        length = np.linalg.norm(chord)
        if length > 1e-9:
            dist = np.abs(chord[0] * rel[:, 1] - chord[1] * rel[:, 0]) / length
        else:
            dist = np.linalg.norm(rel, axis=1)
        i = int(dist.argmax())
        if dist[i] > tol:
            mid = a + 1 + i
            keep.add(mid)
            stack += [(a, mid), (mid, b)]
    return sorted(keep)


def bezier(p0, p1, p2, p3, n=64):
    t = np.linspace(0, 1, n)[:, None]
    u = 1 - t
    return u**3 * p0 + 3 * u**2 * t * p1 + 3 * u * t**2 * p2 + t**3 * p3


def fit(points, tol):
    n = len(points)
    path = np.vstack([points, points[:1]])

    def tangent(i):
        v = points[(i + 4) % n] - points[(i - 4) % n]
        return v / (np.linalg.norm(v) or 1)

    def handles(anchors):
        pts = path[anchors]
        chords = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        lengths = np.append(np.minimum(chords, np.roll(chords, 1)) / 3, 0)
        lengths[-1] = lengths[0]
        dirs = np.array([tangent(i) for i in anchors])
        return pts, dirs * lengths[:, None]

    def worst(a, b, curve):
        d = np.linalg.norm(path[a:b + 1, None] - curve[None], axis=2)
        to_curve, to_path = d.min(1), d.min(0)
        if to_curve.max() >= to_path.max():
            return to_curve.max(), a + int(to_curve.argmax())
        return to_path.max(), a + int(d[:, to_path.argmax()].argmin())

    anchors = simplify(path, tol * 0.8)
    while True:
        pts, h = handles(anchors)
        extra = []
        for j, (a, b) in enumerate(zip(anchors, anchors[1:])):
            curve = bezier(pts[j], pts[j] + h[j], pts[j + 1] - h[j + 1], pts[j + 1])
            err, i = worst(a, b, curve)
            if err > tol and a < i < b:
                extra.append(i)
        if not extra:
            break
        anchors = sorted(set(anchors) | set(extra))

    pts, h = handles(anchors)
    knots = np.rint(pts).astype(int)
    incoming = np.rint(pts - h).astype(int)
    knots[-1], incoming[-1] = knots[0], incoming[0]

    out = [knots[0]]
    for j in range(len(anchors) - 1):
        out += [2 * knots[j] - incoming[j], incoming[j + 1], knots[j + 1]]
    return np.array(out)


def main():
    parser = argparse.ArgumentParser(description="Turn a white-on-black circuit image into Bezier control points.")
    parser.add_argument("image")
    parser.add_argument("-o", "--output", default="savefile.txt")
    parser.add_argument("-s", "--size", type=int, default=1200, help="window size in pixels")
    parser.add_argument("-m", "--margin", type=int, default=50)
    parser.add_argument("-t", "--tolerance", type=float, default=2.5, help="max distance from the track in pixels")
    parser.add_argument("--max-points", type=int, default=1000)
    args = parser.parse_args()

    outline = fit_to_window(resample(load_outline(args.image)), args.size, args.margin)

    tol = args.tolerance
    points = fit(outline, tol)
    while len(points) > args.max_points:
        tol *= 1.25
        points = fit(outline, tol)

    with open(args.output, "w") as f:
        f.write(f"{len(points)}\n")
        f.writelines(f"{x} {y}\n" for x, y in points)
    print(f"{len(points)} points ({len(points) // 3} curves) at {tol:.2f}px -> {args.output}")


if __name__ == "__main__":
    main()
