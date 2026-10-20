from __future__ import annotations

import argparse
from logging import getLogger
from pathlib import Path

from harvesters.core import Harvester

from camera_stream import (
    FX17CameraStreamWorker,
    _create_acquirer,
    _start_acquisition,
    _stop_acquisition,
)


def node_value(node, attribute: str):
    try:
        return getattr(node, attribute)
    except Exception as exc:
        return f"<{type(exc).__name__}>"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read FX17 AcquisitionControl line-rate nodes without acquisition or writes."
    )
    parser.add_argument("cti_path", type=Path)
    parser.add_argument("--device-index", type=int, default=0)
    parser.add_argument(
        "--probe-set-rate",
        type=float,
        help="Temporarily set and read back this rate, then restore prior state.",
    )
    parser.add_argument(
        "--during-acquisition",
        action="store_true",
        help="Run the temporary write probe while acquisition is active.",
    )
    args = parser.parse_args()

    harvester = Harvester(logger=getLogger())
    image_acquirer = None
    try:
        harvester.add_file(str(args.cti_path.resolve()))
        harvester.update()
        print(f"devices={len(harvester.device_info_list)}")
        if not harvester.device_info_list:
            raise RuntimeError("No GenTL device discovered")
        image_acquirer = _create_acquirer(harvester, args.device_index)
        feature_nodes = FX17CameraStreamWorker._gen_feature(
            image_acquirer.remote_device.node_map.AcquisitionControl
        )
        for name in sorted(feature_nodes):
            if not any(
                token in name.lower()
                for token in ("rate", "exposure", "acquisition")
            ):
                continue
            node = feature_nodes[name]
            print(
                f"{name}: value={node_value(node, 'value')!r} "
                f"min={node_value(node, 'min')!r} "
                f"max={node_value(node, 'max')!r} "
                f"inc={node_value(node, 'inc')!r}"
            )
        if args.probe_set_rate is not None:
            features = {"AcquisitionControl": feature_nodes}
            enable, rate, _, _ = FX17CameraStreamWorker._line_rate_nodes(
                features
            )
            previous_enabled = bool(enable.value)
            previous_rate = float(rate.value) if previous_enabled else None
            acquisition_started = False
            try:
                if args.during_acquisition:
                    _start_acquisition(image_acquirer)
                    acquisition_started = True
                status = FX17CameraStreamWorker._write_line_rate(
                    features,
                    args.probe_set_rate,
                )
                print(f"probe_status={status}")
            finally:
                if acquisition_started:
                    _stop_acquisition(image_acquirer)
                if previous_enabled and previous_rate is not None:
                    rate.value = previous_rate
                    enable.value = True
                else:
                    enable.value = False
                print(
                    "restored="
                    f"{FX17CameraStreamWorker._read_line_rate(features)}"
                )
        return 0
    finally:
        if image_acquirer is not None:
            image_acquirer.destroy()
        harvester.reset()


if __name__ == "__main__":
    raise SystemExit(main())
