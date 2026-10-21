from __future__ import annotations

import json
import sys
from datetime import datetime
from pathlib import Path

import numpy as np
from PySide6.QtCore import QRectF, Qt, QTimer
from PySide6.QtGui import (
    QColor,
    QImage,
    QLinearGradient,
    QPainter,
    QPen,
    QPixmap,
)
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QDoubleSpinBox,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from analysis_session_storage import AnalysisSessionStorage, SampleOutputTarget
from live_analysis import LiveAnalysisResult, LiveAnalysisWorker
from geometry_calibration import estimate_geometry_calibration
from recorded_raw_stream import RecordedRawStreamWorker
from replay_backend import RollingLineBuffer
from sample_record import read_envi_wavelengths
from stream_source import StreamMessage, StreamWorker


APP_DIR = Path(__file__).resolve().parent
SETTINGS_PATH = APP_DIR / "settings.example.json"
DEFAULT_WINDOW_WIDTH = 1420
DEFAULT_WINDOW_HEIGHT = 1020
RUNTIME_SETTING_DEFAULTS = {
    "camera_processing_queue_lines": 512,
    "camera_driver_buffer_count": 128,
    "camera_acquisition_log_enabled": True,
    "camera_acquisition_log_dir": "../../acquisition_logs",
    "sample_start_threshold": 0.05,
    "sample_end_threshold": 0.04,
    "max_sample_lines": 1200,
    "save_analysis_captures": False,
    "analysis_capture_dir": "../../analysis_captures",
    "analysis_capture_limit_per_run": 0,
    "save_sample_records": True,
    "sample_record_dir": "../../sample_records",
    "analysis_output_dir": "../../analysis_outputs",
}


def calibration_pair_paths(base_path: str | Path) -> dict[str, Path]:
    """Turn one user-selected base name into the dark/white reference pair."""
    path = Path(base_path)
    suffix = path.suffix or ".dat"
    stem = path.stem if path.suffix else path.name
    for marker in ("_Ref-Dark", "_Ref-White", "-Ref-Dark", "-Ref-White"):
        if stem.casefold().endswith(marker.casefold()):
            stem = stem[: -len(marker)]
            break
    stem = stem.strip()
    if not stem:
        raise ValueError("校正檔名不可為空白")
    return {
        "dark": path.with_name(f"{stem}_Ref-Dark{suffix}"),
        "white": path.with_name(f"{stem}_Ref-White{suffix}"),
    }


def load_settings() -> dict:
    if SETTINGS_PATH.exists():
        settings = json.loads(SETTINGS_PATH.read_text(encoding="utf-8"))
    else:
        settings = {}
    for key, value in RUNTIME_SETTING_DEFAULTS.items():
        settings.setdefault(key, value)
    return settings


def resolve_setting_path(value: str | Path) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = APP_DIR / path
    return path.resolve()


def _shared_pipeline_settings(settings: dict) -> dict:
    spatial_roi_end = settings.get("spatial_roi_end")
    return {
        "false_color_wavelengths_nm": tuple(
            float(value)
            for value in settings.get(
                "false_color_wavelengths_nm",
                [1650.0, 1300.0, 1000.0],
            )
        ),
        "preview_min": float(
            settings.get("preview_reflectance_min", 0.0)
        ),
        "preview_max": float(
            settings.get("preview_reflectance_max", 0.9)
        ),
        "preview_gamma": float(settings.get("preview_gamma", 1.0)),
        "detector_percentile_low": float(
            settings.get("detector_percentile_low", 1.0)
        ),
        "detector_percentile_high": float(
            settings.get("detector_percentile_high", 99.0)
        ),
        "trim_bands": int(settings.get("trim_bands", 10)),
        "clip_max": float(settings.get("clip_max", 1.2)),
        "material_pixel_threshold": float(
            settings.get("material_pixel_threshold", 0.08)
        ),
        "spatial_roi_start": int(settings.get("spatial_roi_start", 0)),
        "spatial_roi_end": (
            None if spatial_roi_end is None else int(spatial_roi_end)
        ),
        "start_threshold": float(
            settings.get("sample_start_threshold", 0.05)
        ),
        "end_threshold": float(
            settings.get("sample_end_threshold", 0.04)
        ),
        "start_confirm_lines": int(
            settings.get("start_confirm_lines", 4)
        ),
        "end_confirm_lines": int(settings.get("end_confirm_lines", 8)),
        "min_sample_lines": int(settings.get("min_sample_lines", 200)),
        "max_sample_lines": int(settings.get("max_sample_lines", 1200)),
    }


def exposure_us_from_settings(settings: dict) -> float:
    if "exposure_ms" in settings:
        return float(settings["exposure_ms"]) * 1000.0
    return float(settings.get("exposure_us", 1200.0))


def stream_error_message(error: str | None) -> str:
    """Turn a worker traceback into a short, actionable detector message."""
    text = (error or "").strip()
    lowered = text.lower()
    if "timed out" in lowered:
        return (
            "FX17 已連線，但未收到影像 buffer\n"
            "請重新啟動 FX17 電源後，再重新開啟程式"
        )
    if "no gentl camera was discovered" in lowered:
        return (
            "找不到 FX17 相機\n"
            "請確認相機電源、網路線，並關閉其他取像程式"
        )
    if "no fx17 camera was found" in lowered:
        return (
            "GenTL 已找到裝置，但其中沒有 FX17\n"
            "請確認使用正確的 CTI、網路介面與相機型號"
        )
    if "multiple fx17 cameras were discovered" in lowered:
        return (
            "偵測到多台 FX17，為避免連錯相機，程式未自動連線\n"
            "請只保留目標相機，或在設定檔指定其序號"
        )
    detail_lines = [line.strip() for line in text.splitlines() if line.strip()]
    detail = detail_lines[-1] if detail_lines else "未知錯誤"
    return f"相機串流錯誤\n{detail}"


def build_stream_worker(settings: dict) -> StreamWorker:
    source = str(settings.get("stream_source", "recorded")).strip().lower()
    shared = _shared_pipeline_settings(settings)
    if source == "recorded":
        return RecordedRawStreamWorker(
            chunk_lines=int(settings.get("simulation_chunk_lines", 20)),
            line_rate=float(settings.get("simulation_line_rate", 250.0)),
            gap_lines=int(settings.get("simulation_gap_lines", 64)),
            **shared,
        )
    if source == "camera":
        from camera_stream import FX17CameraStreamWorker

        geometry_profile = settings.get("geometry_calibration", {})
        saved_line_rate_hz = None
        if (
            geometry_profile.get("valid")
            and geometry_profile.get("correction_method")
            == "fx17_acquisition_line_rate"
            and geometry_profile.get("stream_source", "camera") == "camera"
            and geometry_profile.get("applied_line_rate_hz") is not None
        ):
            saved_line_rate_hz = float(
                geometry_profile["applied_line_rate_hz"]
            )
        return FX17CameraStreamWorker(
            cti_path=resolve_setting_path(
                settings.get(
                    "cti_path",
                    "../fx_test/mvGenTLProducer.cti",
                )
            ),
            dark_ref_path=resolve_setting_path(
                settings.get(
                    "dark_ref_path",
                    "../../20260528_導電材料/50u_1-Ref-Dark.dat",
                )
            ),
            white_ref_path=resolve_setting_path(
                settings.get(
                    "white_ref_path",
                    "../../20260528_導電材料/50u_1-Ref-White.dat",
                )
            ),
            wavelength_hdr_path=resolve_setting_path(
                settings.get(
                    "camera_wavelength_hdr_path",
                    "../../20260528_導電材料/50u_1.hdr",
                )
            ),
            device_index=int(settings.get("camera_device_index", 0)),
            serial_number=settings.get("camera_serial_number"),
            bands=int(settings.get("camera_bands", 224)),
            width=int(settings.get("camera_width", 640)),
            chunk_lines=int(settings.get("camera_chunk_lines", 20)),
            processing_queue_lines=int(
                settings.get("camera_processing_queue_lines", 512)
            ),
            driver_buffer_count=int(
                settings.get("camera_driver_buffer_count", 128)
            ),
            acquisition_log_enabled=bool(
                settings.get("camera_acquisition_log_enabled", True)
            ),
            acquisition_log_dir=resolve_setting_path(
                settings.get(
                    "camera_acquisition_log_dir",
                    "../../acquisition_logs",
                )
            ),
            fetch_timeout=float(settings.get("camera_fetch_timeout", 0.5)),
            max_consecutive_timeouts=int(
                settings.get("camera_max_consecutive_timeouts", 20)
            ),
            discovery_attempts=int(
                settings.get("camera_discovery_attempts", 12)
            ),
            discovery_retry_interval=float(
                settings.get("camera_discovery_retry_interval", 1.0)
            ),
            calibration_frame_count=int(
                settings.get("camera_calibration_frame_count", 64)
            ),
            calibration_settle_frames=int(
                settings.get("camera_calibration_settle_frames", 8)
            ),
            calibration_settle_seconds=float(
                settings.get("camera_calibration_settle_seconds", 0.5)
            ),
            configure_camera=bool(
                settings.get("camera_configure_on_connect", True)
            ),
            exposure_us=exposure_us_from_settings(settings),
            line_rate_hz=saved_line_rate_hz,
            open_shutter_on_start=bool(
                settings.get("camera_open_shutter_on_start", True)
            ),
            close_shutter_on_pause=bool(
                settings.get("camera_close_shutter_on_pause", True)
            ),
            **shared,
        )
    raise ValueError(
        f"Unsupported stream_source {source!r}; expected 'recorded' or 'camera'."
    )


def array_to_pixmap(image: np.ndarray) -> QPixmap:
    image = np.ascontiguousarray(image.astype(np.uint8))
    height, width, channels = image.shape
    qimage = QImage(
        image.data,
        width,
        height,
        width * channels,
        QImage.Format.Format_RGB888,
    ).copy()
    return QPixmap.fromImage(qimage)


class AspectRatioImageLabel(QLabel):
    """Always scale the original image to the label's current viewport."""

    def __init__(
        self,
        aspect_mode: Qt.AspectRatioMode,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._source_pixmap = QPixmap()
        self._aspect_mode = aspect_mode

    def set_source_pixmap(self, pixmap: QPixmap) -> None:
        self._source_pixmap = pixmap
        self.refresh_image()

    def clear_source(self) -> None:
        self._source_pixmap = QPixmap()
        super().clear()

    def refresh_image(self) -> None:
        if self._source_pixmap.isNull() or self.width() < 1 or self.height() < 1:
            return
        super().setPixmap(
            self._source_pixmap.scaled(
                self.size(),
                self._aspect_mode,
                Qt.TransformationMode.SmoothTransformation,
            )
        )

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        self.refresh_image()


class DetectorCanvas(QWidget):
    """Centered detector image with spectral/spatial axes and a DN colorbar."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("detectorCanvas")
        self._pixmap = QPixmap()
        self._message = "等待光譜幀"
        self._value_range = (0.0, 1.0)
        self._spectral_range_nm: tuple[float, float] | None = None
        self._spatial_pixels = 640
        self.setMinimumHeight(205)
        self.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Expanding,
        )

    def set_frame(
        self,
        pixmap: QPixmap,
        *,
        value_range: tuple[float, float] | None,
        spectral_range_nm: tuple[float, float] | None,
        spatial_pixels: int,
    ) -> None:
        self._pixmap = pixmap
        self._message = ""
        if value_range is not None:
            self._value_range = tuple(float(value) for value in value_range)
        self._spectral_range_nm = spectral_range_nm
        self._spatial_pixels = max(int(spatial_pixels), 1)
        self.update()

    def set_message(self, message: str) -> None:
        self._pixmap = QPixmap()
        self._message = message
        self.update()

    def paintEvent(self, event) -> None:  # noqa: N802
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor("#111111"))
        if self._pixmap.isNull():
            painter.setPen(QColor("#d6b36a"))
            painter.drawText(
                self.rect(),
                Qt.AlignmentFlag.AlignCenter,
                self._message,
            )
            return

        left_margin = 82.0
        right_margin = 54.0
        top_margin = 8.0
        bottom_margin = 42.0
        bar_width = 18.0
        bar_gap = 14.0
        bar_label_width = 42.0
        usable_width = max(
            self.width() - left_margin - right_margin,
            1.0,
        )
        usable_height = max(
            self.height() - top_margin - bottom_margin,
            1.0,
        )
        image_max_width = max(
            usable_width - bar_width - bar_gap - bar_label_width,
            1.0,
        )
        display_aspect = 2.05
        image_width = min(image_max_width, usable_height * display_aspect)
        image_height = image_width / display_aspect
        composite_width = (
            image_width + bar_gap + bar_width + bar_label_width
        )
        image_left = left_margin + max(
            (usable_width - composite_width) / 2.0,
            0.0,
        )
        image_top = top_margin + max(
            (usable_height - image_height) / 2.0,
            0.0,
        )
        image_rect = QRectF(
            image_left,
            image_top,
            image_width,
            image_height,
        )
        painter.drawPixmap(image_rect, self._pixmap, QRectF(self._pixmap.rect()))
        painter.setPen(QPen(QColor("#b7b7b7"), 1.0))
        painter.drawRect(image_rect)

        tick_color = QColor("#a9a9a9")
        painter.setPen(tick_color)
        font = painter.font()
        font.setPointSizeF(8.0)
        painter.setFont(font)
        for index in range(5):
            fraction = index / 4.0
            x = image_rect.left() + fraction * image_rect.width()
            painter.drawLine(
                int(x),
                int(image_rect.bottom()),
                int(x),
                int(image_rect.bottom() + 4),
            )
            value = round(fraction * (self._spatial_pixels - 1))
            painter.drawText(
                QRectF(x - 24, image_rect.bottom() + 5, 48, 16),
                Qt.AlignmentFlag.AlignHCenter,
                str(value),
            )
        painter.drawText(
            QRectF(
                image_rect.left(),
                self.height() - 22,
                image_rect.width(),
                18,
            ),
            Qt.AlignmentFlag.AlignCenter,
            "spatial [pixel]",
        )

        spectral_range = self._spectral_range_nm
        spectral_label = "spectral [nm]"
        if spectral_range is None:
            spectral_range = (0.0, float(self._pixmap.height() - 1))
            spectral_label = "spectral [band]"
        spectral_low, spectral_high = sorted(spectral_range)
        y_tick_width = 42.0
        y_tick_gap = 8.0
        for index in range(5):
            fraction = index / 4.0
            y = image_rect.top() + fraction * image_rect.height()
            value = spectral_high - fraction * (
                spectral_high - spectral_low
            )
            painter.drawLine(
                int(image_rect.left() - 4),
                int(y),
                int(image_rect.left()),
                int(y),
            )
            painter.drawText(
                QRectF(
                    image_rect.left() - y_tick_gap - y_tick_width,
                    y - 8,
                    y_tick_width,
                    16,
                ),
                Qt.AlignmentFlag.AlignRight
                | Qt.AlignmentFlag.AlignVCenter,
                f"{value:.0f}",
            )
        spectral_label_width = 18.0
        spectral_label_gap = 5.0
        spectral_label_center_x = (
            image_rect.left()
            - y_tick_gap
            - y_tick_width
            - spectral_label_gap
            - spectral_label_width / 2.0
        )
        painter.save()
        painter.translate(
            max(spectral_label_center_x, spectral_label_width / 2.0),
            image_rect.center().y(),
        )
        painter.rotate(-90)
        painter.drawText(
            QRectF(
                -image_rect.height() / 2,
                0,
                image_rect.height(),
                18,
            ),
            Qt.AlignmentFlag.AlignCenter,
            spectral_label,
        )
        painter.restore()

        bar_rect = QRectF(
            image_rect.right() + bar_gap,
            image_rect.top(),
            bar_width,
            image_rect.height(),
        )
        gradient = QLinearGradient(
            0,
            bar_rect.top(),
            0,
            bar_rect.bottom(),
        )
        gradient.setColorAt(0.0, QColor("#ffffff"))
        gradient.setColorAt(1.0, QColor("#000000"))
        painter.fillRect(bar_rect, gradient)
        painter.setPen(QPen(QColor("#b7b7b7"), 1.0))
        painter.drawRect(bar_rect)
        value_low, value_high = sorted(self._value_range)
        for index in range(5):
            fraction = index / 4.0
            y = bar_rect.top() + fraction * bar_rect.height()
            value = value_high - fraction * (value_high - value_low)
            painter.drawText(
                QRectF(
                    bar_rect.right() + 5,
                    y - 8,
                    bar_label_width - 5,
                    16,
                ),
                Qt.AlignmentFlag.AlignLeft
                | Qt.AlignmentFlag.AlignVCenter,
                f"{value:.0f}",
            )


class DetectorPanel(QFrame):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._pixmap = QPixmap()
        self.setObjectName("panel")
        self.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Expanding,
        )
        self.eyebrow_label = QLabel("DETECTOR")
        self.eyebrow_label.setObjectName("eyebrow")
        self.title_label = QLabel("光譜 × 空間")
        self.title_label.setObjectName("panelTitle")
        header = QVBoxLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.setSpacing(2)
        header.addWidget(self.eyebrow_label)
        header.addWidget(self.title_label)
        self.canvas = DetectorCanvas()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(7)
        layout.addLayout(header)
        layout.addWidget(self.canvas, 1)

    def set_title(self, title: str) -> None:
        self.title_label.setText(title)

    def set_image(
        self,
        image: np.ndarray,
        *,
        value_range: tuple[float, float] | None = None,
        spectral_range_nm: tuple[float, float] | None = None,
    ) -> None:
        self._pixmap = array_to_pixmap(image)
        self.canvas.set_frame(
            self._pixmap,
            value_range=value_range,
            spectral_range_nm=spectral_range_nm,
            spatial_pixels=image.shape[1],
        )

    def set_message(self, message: str) -> None:
        self._pixmap = QPixmap()
        self.canvas.set_message(message)


class ImagePanel(QFrame):
    def __init__(
        self,
        eyebrow: str,
        title: str,
        parent: QWidget | None = None,
        *,
        aspect_mode: Qt.AspectRatioMode = Qt.AspectRatioMode.KeepAspectRatio,
    ) -> None:
        super().__init__(parent)
        self._pixmap = QPixmap()
        self._aspect_mode = aspect_mode
        self.setObjectName("panel")
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        self.eyebrow_label = QLabel(eyebrow)
        self.eyebrow_label.setObjectName("eyebrow")
        self.title_label = QLabel(title)
        self.title_label.setObjectName("panelTitle")

        header = QVBoxLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.setSpacing(2)
        header.addWidget(self.eyebrow_label)
        header.addWidget(self.title_label)

        self.image_label = AspectRatioImageLabel(self._aspect_mode)
        self.image_label.setObjectName("imageViewport")
        self.image_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.image_label.setSizePolicy(
            QSizePolicy.Policy.Ignored,
            QSizePolicy.Policy.Ignored,
        )

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(10)
        layout.addLayout(header)
        layout.addWidget(self.image_label, 1)

    def set_title(self, title: str) -> None:
        self.title_label.setText(title)

    def set_image(self, image: np.ndarray) -> None:
        self.set_pixmap(array_to_pixmap(image))

    def set_pixmap(self, pixmap: QPixmap) -> None:
        self.image_label.setText("")
        self._pixmap = pixmap
        self.image_label.set_source_pixmap(pixmap)

    def set_message(self, message: str) -> None:
        self._pixmap = QPixmap()
        self.image_label.clear_source()
        self.image_label.setText(message)

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        self._refresh_pixmap()

    def _refresh_pixmap(self) -> None:
        if self._pixmap.isNull():
            return
        self.image_label.refresh_image()


class MetricCard(QFrame):
    def __init__(self, title: str, unit: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("metricCard")

        title_label = QLabel(title)
        title_label.setObjectName("metricTitle")
        self.value_label = QLabel("--")
        self.value_label.setObjectName("metricValue")
        unit_label = QLabel(unit)
        unit_label.setObjectName("metricUnit")

        value_row = QHBoxLayout()
        value_row.setContentsMargins(0, 0, 0, 0)
        value_row.setSpacing(8)
        value_row.addWidget(self.value_label)
        value_row.addWidget(unit_label, 0, Qt.AlignmentFlag.AlignBottom)
        value_row.addStretch(1)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(8)
        layout.addWidget(title_label)
        layout.addLayout(value_row)

    def set_value(self, value: float | None, decimals: int = 1) -> None:
        self.value_label.setText("--" if value is None else f"{value:.{decimals}f}")


class ControlSidebar(QFrame):
    def __init__(self, settings: dict, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("sidebar")
        self.setFixedWidth(230)

        self.exposure = QDoubleSpinBox()
        self.exposure.setRange(0.01, 500.0)
        self.exposure.setDecimals(3)
        self.exposure.setSingleStep(0.1)
        self.exposure.setSuffix(" ms")
        self.exposure.setValue(exposure_us_from_settings(settings) / 1000.0)
        source = str(settings.get("stream_source", "recorded")).strip().lower()
        self.camera_identity_status = QLabel(
            "相機：正在搜尋…" if source == "camera" else "來源：Recorded"
        )
        self.camera_identity_status.setWordWrap(True)
        self.camera_identity_status.setObjectName("fieldLabel")

        self.start_button = self._button("開始", "primaryButton")
        self.stop_button = self._button("停止", "secondaryButton")
        self.capture_calibration_button = self._button(
            "拍攝黑／白校正",
            "secondaryButton",
        )
        self.select_calibration_files_button = self._button(
            "選擇黑／白校正檔",
            "secondaryButton",
        )
        self.geometry_calibration_button = self._button(
            "開始校正",
            "primaryButton",
        )
        self.select_analysis_output_button = self._button(
            "選擇儲存路徑",
            "secondaryButton",
        )
        geometry_settings = settings.get("geometry_calibration", {})
        applied_rate = geometry_settings.get("applied_line_rate_hz")
        if geometry_settings.get("valid") and applied_rate is not None:
            geometry_status = f"已套用 line rate {float(applied_rate):.3f} Hz"
        elif "scan_scale" in geometry_settings:
            geometry_status = (
                "舊版影像拉伸已停用，請重新進行推掃取像參數校正"
            )
        elif geometry_settings.get("invalid_reason"):
            geometry_status = (
                f"{geometry_settings['invalid_reason']}，"
                "請重新進行推掃取像參數校正"
            )
        else:
            geometry_status = "尚未進行推掃取像參數校正"
        self.geometry_calibration_status = QLabel(geometry_status)
        self.geometry_calibration_status.setWordWrap(True)
        self.geometry_calibration_status.setObjectName("fieldLabel")
        self.white_ref = self._path_display(
            str(settings.get("white_ref_path", ""))
        )
        self.dark_ref = self._path_display(
            str(settings.get("dark_ref_path", ""))
        )
        self.analysis_output = self._path_display(
            str(settings.get("analysis_output_dir", "../../analysis_outputs"))
        )

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)
        layout.addWidget(self._camera_group())
        layout.addWidget(self._calibration_group())
        layout.addWidget(self._geometry_calibration_group())
        layout.addWidget(self._analysis_group())
        layout.addStretch(1)

    @staticmethod
    def _button(text: str, object_name: str) -> QPushButton:
        button = QPushButton(text)
        button.setObjectName(object_name)
        return button

    def _group(self, title: str) -> tuple[QFrame, QVBoxLayout]:
        frame = QFrame()
        frame.setObjectName("controlGroup")
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(9, 9, 9, 9)
        layout.setSpacing(7)
        title_label = QLabel(title)
        title_label.setObjectName("groupTitle")
        title_label.setWordWrap(True)
        layout.addWidget(title_label)
        return frame, layout

    def _camera_group(self) -> QFrame:
        frame, layout = self._group("相機控制")
        layout.addWidget(self.camera_identity_status)
        layout.addWidget(self._labeled_widget("曝光時間", self.exposure))
        return frame

    def _calibration_group(self) -> QFrame:
        frame, layout = self._group("黑白校正")
        layout.addWidget(self._labeled_widget("白校正檔", self.white_ref))
        layout.addWidget(self._labeled_widget("黑校正檔", self.dark_ref))
        layout.addWidget(self.select_calibration_files_button)
        layout.addWidget(self.capture_calibration_button)
        return frame

    def _geometry_calibration_group(self) -> QFrame:
        frame, layout = self._group("推掃取像參數校正（形變校正）")
        layout.addWidget(self.geometry_calibration_status)
        layout.addWidget(self.geometry_calibration_button)
        return frame

    def _analysis_group(self) -> QFrame:
        frame, layout = self._group("檢測分析")
        layout.addWidget(
            self._labeled_widget("分析結果儲存路徑", self.analysis_output)
        )
        layout.addWidget(self.select_analysis_output_button)
        layout.addLayout(self._button_row(self.start_button, self.stop_button))
        return frame

    @staticmethod
    def _button_row(*buttons: QPushButton) -> QHBoxLayout:
        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(6)
        for button in buttons:
            row.addWidget(button)
        return row

    def _path_display(self, value: str) -> QWidget:
        holder = QWidget()
        layout = QHBoxLayout(holder)
        layout.setContentsMargins(0, 0, 0, 0)
        line_edit = QLineEdit(value)
        line_edit.setReadOnly(True)
        holder.line_edit = line_edit  # type: ignore[attr-defined]
        layout.addWidget(line_edit, 1)
        return holder

    @staticmethod
    def _labeled_widget(label: str, widget: QWidget) -> QWidget:
        holder = QWidget()
        layout = QVBoxLayout(holder)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        title = QLabel(label)
        title.setObjectName("fieldLabel")
        layout.addWidget(title)
        layout.addWidget(widget)
        return holder


class MainWindow(QMainWindow):
    def __init__(self, stream_worker: StreamWorker | None = None) -> None:
        super().__init__()
        self.settings = load_settings()
        self.analysis_failed = False
        self.running = False
        self.current_sample_id: str | None = None
        self.last_result_sample_id: str | None = None
        self.calibration_in_progress: str | None = None
        self.calibration_pair_paths: dict[str, Path] | None = None
        self.calibration_file_selection_pending = False
        self.geometry_calibration_active = False
        self.geometry_calibration_was_running = False
        self.geometry_calibration_phase: str | None = None
        self.geometry_calibration_current_line_rate_hz: float | None = None
        self.geometry_calibration_line_rate_min_hz: float | None = None
        self.geometry_calibration_line_rate_max_hz: float | None = None
        self.geometry_calibration_result = None
        self.geometry_calibration_sample_id: str | None = None
        self.pending_cubes: list[
            tuple[
                str,
                np.ndarray,
                np.ndarray | None,
                dict,
                SampleOutputTarget | None,
            ]
        ] = []
        self.analysis_session: AnalysisSessionStorage | None = None
        self.analysis_session_stop_requested_at: datetime | None = None
        self.sample_started_at: dict[str, datetime] = {}
        self.session_outstanding_records: set[str] = set()
        self.live_buffer = RollingLineBuffer(width=640)
        spatial_roi_end = self.settings.get("spatial_roi_end")
        capture_enabled = bool(
            self.settings.get("save_analysis_captures", True)
        )
        record_enabled = bool(self.settings.get("save_sample_records", True))
        source_wavelength_hdr = resolve_setting_path(
            self.settings.get(
                "camera_wavelength_hdr_path",
                "../../20260528_導電材料/50u_1.hdr",
            )
        )
        trim_bands = int(self.settings.get("trim_bands", 10))
        output_bands = int(self.settings.get("camera_bands", 224)) - 2 * trim_bands
        raw_record_wavelengths = read_envi_wavelengths(
            source_wavelength_hdr,
            trim_bands=0,
            expected_bands=int(self.settings.get("camera_bands", 224)),
        )
        record_wavelengths = read_envi_wavelengths(
            source_wavelength_hdr,
            trim_bands=trim_bands,
            expected_bands=output_bands,
        )
        self.analysis_worker = LiveAnalysisWorker(
            device=str(self.settings.get("analysis_device", "cuda")),
            spatial_roi_start=int(self.settings.get("spatial_roi_start", 0)),
            spatial_roi_end=(
                None if spatial_roi_end is None else int(spatial_roi_end)
            ),
            capture_dir=(
                resolve_setting_path(
                    self.settings.get(
                        "analysis_capture_dir",
                        "../../analysis_captures",
                    )
                )
                if capture_enabled
                else None
            ),
            capture_limit=int(
                self.settings.get("analysis_capture_limit_per_run", 0)
            ),
            sample_record_dir=(
                resolve_setting_path(
                    self.settings.get("sample_record_dir", "../../sample_records")
                )
                if record_enabled
                else None
            ),
            sample_wavelengths_nm=record_wavelengths,
            raw_sample_wavelengths_nm=raw_record_wavelengths,
            sample_sensor_type=str(
                self.settings.get("camera_model") or "Specim FX17e"
            ),
        )
        self.stream_worker = stream_worker or build_stream_worker(self.settings)
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(False)
        else:
            self.stream_worker.pause()

        self.setWindowTitle("光譜影像均勻度動態分析軟體")
        screen = QApplication.primaryScreen()
        if screen is None:
            self.resize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)
        else:
            available = screen.availableGeometry()
            self.resize(
                min(DEFAULT_WINDOW_WIDTH, available.width()),
                min(DEFAULT_WINDOW_HEIGHT, available.height()),
            )

        central = QWidget()
        root = QHBoxLayout(central)
        root.setContentsMargins(14, 14, 14, 14)
        root.setSpacing(10)
        self.root_layout = root
        self.setCentralWidget(central)

        self.sidebar = ControlSidebar(self.settings)
        self.sidebar.start_button.clicked.connect(self.start_stream)
        self.sidebar.stop_button.clicked.connect(self.stop_stream)
        self.sidebar.select_analysis_output_button.clicked.connect(
            self._select_analysis_output_directory
        )
        self.sidebar.capture_calibration_button.clicked.connect(
            self._start_calibration_pair
        )
        self.sidebar.select_calibration_files_button.clicked.connect(
            self._select_calibration_files
        )
        self.sidebar.geometry_calibration_button.clicked.connect(
            self._toggle_geometry_calibration
        )
        self.sidebar.exposure.valueChanged.connect(
            self._set_camera_exposure
        )
        capture_supported = callable(
            getattr(self.stream_worker, "capture_reference", None)
        )
        self.sidebar.capture_calibration_button.setEnabled(capture_supported)
        selection_supported = callable(
            getattr(self.stream_worker, "set_calibration_files", None)
        )
        self.sidebar.select_calibration_files_button.setEnabled(
            selection_supported
        )
        root.addWidget(self.sidebar)

        main_area = QVBoxLayout()
        main_area.setContentsMargins(0, 0, 0, 0)
        main_area.setSpacing(10)
        self.main_area_layout = main_area
        root.addLayout(main_area, 1)

        self.detector_panel = DetectorPanel()
        self.detector_panel.setMinimumHeight(270)
        self.detector_panel.setMaximumHeight(335)
        main_area.addWidget(self.detector_panel)

        self.live_panel = ImagePanel("即時偽色影像", "載入樣本")
        self.result_panel = ImagePanel("分析結果", "均勻度結果")
        # The former 420 x 360 minimum made the child image viewport taller than
        # its panel on short/high-DPI screens.  The parent then clipped the lower
        # part of an otherwise complete pixmap.  A zero minimum lets the layout
        # shrink the viewport; AspectRatioImageLabel still fits the whole image.
        self.live_panel.image_label.setMinimumSize(0, 0)
        self.result_panel.image_label.setMinimumSize(0, 0)

        image_row = QHBoxLayout()
        image_row.setContentsMargins(0, 0, 0, 0)
        image_row.setSpacing(10)
        self.image_row_layout = image_row
        image_row.addWidget(self.live_panel, 1)
        image_row.addWidget(self.result_panel, 1)
        main_area.addLayout(image_row, 1)

        metrics = QHBoxLayout()
        metrics.setSpacing(10)
        self.metrics_layout = metrics
        self.uniformity_card = MetricCard("均勻度", "%")
        self.resistance_card = MetricCard("電阻值", "Ω")
        metrics.addWidget(self.uniformity_card, 1)
        metrics.addWidget(self.resistance_card, 1)
        main_area.addLayout(metrics)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
        self.timer.start(80)
        self.live_panel.set_image(self.live_buffer.frame())
        self.live_panel.set_message("按下開始後顯示即時分析影像")
        if str(self.settings.get("stream_source", "recorded")).lower() == "camera":
            self.detector_panel.set_message("正在連接 FX17，等待光譜幀…")
        else:
            self.detector_panel.set_message("等待光譜幀")
        self._set_calibration_controls(True)
        self._last_compact_layout: bool | None = None
        self._apply_responsive_layout()

    def resizeEvent(self, event) -> None:  # noqa: N802
        super().resizeEvent(event)
        if hasattr(self, "detector_panel"):
            self._apply_responsive_layout()

    def _apply_responsive_layout(self) -> None:
        """Keep every panel inside the usable window on short/high-DPI screens."""
        window_height = max(self.height(), 1)
        compact = window_height < 900

        # Detector height scales continuously with the available window height.
        # This leaves enough room for both image panels and the metric cards.
        detector_max = max(190, min(335, int(window_height * 0.31)))
        detector_min = max(150, min(270, detector_max - 30))
        canvas_min = max(105, detector_min - 60)
        self.detector_panel.canvas.setMinimumHeight(canvas_min)
        self.detector_panel.setMinimumHeight(detector_min)
        self.detector_panel.setMaximumHeight(detector_max)

        if compact == self._last_compact_layout:
            return
        self._last_compact_layout = compact

        outer_margin = 8 if compact else 14
        spacing = 6 if compact else 10
        panel_margin = 8 if compact else 14
        self.root_layout.setContentsMargins(
            outer_margin,
            outer_margin,
            outer_margin,
            outer_margin,
        )
        self.root_layout.setSpacing(spacing)
        self.main_area_layout.setSpacing(spacing)
        self.image_row_layout.setSpacing(spacing)
        self.metrics_layout.setSpacing(spacing)

        for panel in (self.live_panel, self.result_panel):
            panel.layout().setContentsMargins(
                panel_margin,
                panel_margin,
                panel_margin,
                panel_margin,
            )
            panel.layout().setSpacing(6 if compact else 10)
            panel.image_label.setMinimumSize(0, 0)

        detector_margin_y = 8 if compact else 12
        self.detector_panel.layout().setContentsMargins(
            panel_margin,
            detector_margin_y,
            panel_margin,
            detector_margin_y,
        )
        self.detector_panel.layout().setSpacing(5 if compact else 7)

    def start_stream(self) -> None:
        if self.running:
            return
        if self.analysis_session is not None:
            QMessageBox.information(
                self,
                "正在完成上一個工作階段",
                "上一個工作階段仍有樣本等待分析與存檔，完成後才能再次開始。",
            )
            return
        try:
            self._start_analysis_session()
        except Exception as exc:
            QMessageBox.critical(
                self,
                "無法建立儲存資料夾",
                f"請確認分析結果儲存路徑可寫入：\n{exc}",
            )
            return
        self.running = True
        self._set_camera_exposure(self.sidebar.exposure.value())
        self.stream_worker.resume()
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(True)
        self.live_buffer.clear()
        self.sample_started_at.clear()
        self.live_panel.set_image(self.live_buffer.frame())
        self.live_panel.set_message("等待即時偽色影像")
        self._set_calibration_controls(True)

    def stop_stream(self) -> None:
        if not self.running:
            return
        self.running = False
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(False)
        else:
            self.stream_worker.pause()
        self.analysis_session_stop_requested_at = datetime.now()
        self.sample_started_at.clear()
        if self.session_outstanding_records or self.pending_cubes:
            self.live_panel.set_message("已停止取像；正在完成已拍攝樣本的分析與存檔")
        else:
            self.live_panel.set_message("檢測分析已停止")
        self._maybe_finalize_analysis_session()
        self._set_calibration_controls(True)

    def _analysis_output_root(self) -> Path:
        text = self.sidebar.analysis_output.line_edit.text().strip()  # type: ignore[attr-defined]
        if not text:
            raise ValueError("尚未選擇分析結果儲存路徑")
        path = Path(text)
        if not path.is_absolute():
            path = APP_DIR / path
        return path.resolve()

    def _start_analysis_session(self) -> None:
        output_root = self._analysis_output_root()
        dark_text = self.sidebar.dark_ref.line_edit.text().strip()  # type: ignore[attr-defined]
        white_text = self.sidebar.white_ref.line_edit.text().strip()  # type: ignore[attr-defined]
        dark_path = resolve_setting_path(dark_text) if dark_text else None
        white_path = resolve_setting_path(white_text) if white_text else None
        self.analysis_session = AnalysisSessionStorage(
            output_root,
            started_at=datetime.now(),
            dark_ref_path=dark_path,
            white_ref_path=white_path,
        )
        self.analysis_session_stop_requested_at = None
        self.session_outstanding_records.clear()

    def _maybe_finalize_analysis_session(self) -> None:
        if (
            self.analysis_session is None
            or self.running
            or self.analysis_session_stop_requested_at is None
            or self.pending_cubes
            or self.session_outstanding_records
        ):
            return
        workbook_path = self.analysis_session.workbook_path
        try:
            self.analysis_session.finish(self.analysis_session_stop_requested_at)
            self.live_panel.set_message(
                f"檢測分析已停止；Excel已保存：{workbook_path.name}"
            )
        except Exception as exc:
            QMessageBox.warning(
                self,
                "Excel儲存失敗",
                f"樣本檔案已保存，但Excel總表無法完成：\n{exc}",
            )
        finally:
            self.analysis_session = None
            self.analysis_session_stop_requested_at = None
            self._set_calibration_controls(True)

    def _toggle_geometry_calibration(self) -> None:
        if self.geometry_calibration_active:
            self._cancel_geometry_calibration()
            return
        if (
            self.calibration_in_progress is not None
            or self.calibration_file_selection_pending
        ):
            QMessageBox.information(
                self,
                "推掃取像參數校正（形變校正）",
                "請先完成目前的黑／白校正作業。",
            )
            return
        request_line_rate = getattr(
            self.stream_worker,
            "request_line_rate",
            None,
        )
        set_line_rate = getattr(
            self.stream_worker,
            "set_line_rate_hz",
            None,
        )
        if not callable(request_line_rate) or not callable(set_line_rate):
            QMessageBox.information(
                self,
                "推掃取像參數校正（形變校正）",
                "目前的串流來源不支援讀寫 line rate，無法進行取像參數校正。",
            )
            return
        answer = QMessageBox.question(
            self,
            "準備推掃取像參數校正",
            "請先讓輸送帶達到穩定速度。\n\n"
            "按下確定後，將平整的圓形標準件完整放上輸送帶；"
            "程式會先讀取 FX17 line rate，再於標準件離開視野後"
            "計算、寫入並讀回新的 line rate。",
            QMessageBox.StandardButton.Ok
            | QMessageBox.StandardButton.Cancel,
            QMessageBox.StandardButton.Ok,
        )
        if answer != QMessageBox.StandardButton.Ok:
            return

        self.geometry_calibration_active = True
        self.geometry_calibration_was_running = self.running
        self.geometry_calibration_phase = "awaiting_line_rate"
        self.geometry_calibration_current_line_rate_hz = None
        self.geometry_calibration_line_rate_min_hz = None
        self.geometry_calibration_line_rate_max_hz = None
        self.geometry_calibration_result = None
        self.geometry_calibration_sample_id = None
        self.pending_cubes.clear()
        reset = getattr(self.stream_worker, "reset_analysis", None)
        if callable(reset):
            reset()
        self.stream_worker.resume()
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(False)
        self.live_buffer.clear()
        self.live_panel.set_image(self.live_buffer.frame())
        self.live_panel.set_message("正在讀取 FX17 line rate…")
        self.result_panel.set_message("推掃取像參數校正進行中")
        self.sidebar.geometry_calibration_status.setText(
            "正在讀取目前的 FX17 line rate，請暫時不要放入標準件"
        )
        self.sidebar.geometry_calibration_button.setText("取消校正")
        self._set_calibration_controls(False)
        request_line_rate()

    def _cancel_geometry_calibration(self) -> None:
        if not self.geometry_calibration_active:
            return
        self.geometry_calibration_active = False
        self.geometry_calibration_phase = None
        self.geometry_calibration_current_line_rate_hz = None
        self.geometry_calibration_line_rate_min_hz = None
        self.geometry_calibration_line_rate_max_hz = None
        self.geometry_calibration_result = None
        self.geometry_calibration_sample_id = None
        self.pending_cubes.clear()
        reset = getattr(self.stream_worker, "reset_analysis", None)
        if callable(reset):
            reset()
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(self.geometry_calibration_was_running)
        elif not self.geometry_calibration_was_running:
            self.stream_worker.pause()
        self.sidebar.geometry_calibration_button.setText("開始校正")
        self.sidebar.geometry_calibration_status.setText("校正已取消")
        self.live_panel.set_message(
            "等待即時分析影像"
            if self.geometry_calibration_was_running
            else "檢測分析已停止"
        )
        self._set_calibration_controls(True)

    def _accept_geometry_calibration(self, message: StreamMessage) -> None:
        if (
            message.cube is None
            or self.geometry_calibration_phase != "capturing_standard"
        ):
            return
        try:
            geometry_cube = message.cube
            configured_width = int(
                self.settings.get("camera_width", geometry_cube.shape[1])
            )
            if geometry_cube.shape[1] == configured_width:
                roi_start = int(self.settings.get("spatial_roi_start", 0))
                roi_end_value = self.settings.get("spatial_roi_end")
                roi_end = (
                    configured_width
                    if roi_end_value is None
                    else int(roi_end_value)
                )
                if not 0 <= roi_start < roi_end <= configured_width:
                    raise ValueError("設定的 spatial ROI 超出 cube 寬度")
                geometry_cube = geometry_cube[:, roi_start:roi_end, :]
            result = estimate_geometry_calibration(geometry_cube)
        except Exception as exc:
            self.sidebar.geometry_calibration_status.setText(
                "未接受這次標準件，請重新放入：" + str(exc)
            )
            self.result_panel.set_message("圓形辨識失敗，等待下一個標準件")
            return
        current_hz = self.geometry_calibration_current_line_rate_hz
        minimum_hz = self.geometry_calibration_line_rate_min_hz
        maximum_hz = self.geometry_calibration_line_rate_max_hz
        if current_hz is None or minimum_hz is None or maximum_hz is None:
            self._fail_geometry_calibration("尚未取得完整的 FX17 line rate 範圍。")
            return
        target_hz = current_hz * result.line_rate_multiplier
        self.result_panel.set_title("圓形標準件偵測結果")
        self.result_panel.set_image(result.preview)
        exposure_ms = float(self.sidebar.exposure.value())
        if target_hz > maximum_hz:
            self._fail_geometry_calibration(
                f"圓形辨識結果需要 {target_hz:.3f} Hz，但目前曝光時間 "
                f"{exposure_ms:.3f} ms 下，FX17 回報的最大值為 "
                f"{maximum_hz:.3f} Hz。\n\n"
                "請降低曝光時間後重新校正；若影像變暗，請增加照明。"
                "也可以降低輸送帶速度。\n\n"
                "程式未修改曝光時間或 line rate。"
            )
            return
        if target_hz < minimum_hz:
            self._fail_geometry_calibration(
                f"圓形辨識結果需要 {target_hz:.3f} Hz，但 FX17 回報的"
                f"最小值為 {minimum_hz:.3f} Hz。\n\n"
                "降低曝光時間無法解決這個情況；請提高輸送帶速度後"
                "重新校正。\n\n"
                "程式未修改曝光時間或 line rate。"
            )
            return
        setter = getattr(self.stream_worker, "set_line_rate_hz", None)
        if not callable(setter):
            self._fail_geometry_calibration("串流來源無法寫入 FX17 line rate。")
            return
        self.geometry_calibration_result = result
        self.geometry_calibration_sample_id = message.sample_id
        self.geometry_calibration_phase = "applying_line_rate"
        self.sidebar.geometry_calibration_status.setText(
            f"H/W {result.height_width_ratio:.3f}；正在將 line rate "
            f"由 {current_hz:.3f} 改為 {target_hz:.3f} Hz…"
        )
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(False)
        setter(target_hz)

    def _finish_geometry_line_rate(self, message: StreamMessage) -> None:
        result = self.geometry_calibration_result
        previous_hz = self.geometry_calibration_current_line_rate_hz
        applied_hz = message.line_rate_hz
        requested_hz = message.line_rate_requested_hz
        if (
            not self.geometry_calibration_active
            or self.geometry_calibration_phase != "applying_line_rate"
            or result is None
            or previous_hz is None
            or applied_hz is None
            or requested_hz is None
        ):
            return
        profile = {
            "valid": True,
            "correction_method": "fx17_acquisition_line_rate",
            "stream_source": str(
                self.settings.get("stream_source", "recorded")
            ).lower(),
            "previous_line_rate_hz": previous_hz,
            "requested_line_rate_hz": requested_hz,
            "applied_line_rate_hz": applied_hz,
            "line_rate_multiplier": result.line_rate_multiplier,
            "line_rate_control_enabled": message.line_rate_control_enabled,
            "line_rate_min_hz": message.line_rate_min_hz,
            "line_rate_max_hz": message.line_rate_max_hz,
            "height_width_ratio": result.height_width_ratio,
            "width_px": result.width_px,
            "height_lines": result.height_lines,
            "ellipse_fill": result.ellipse_fill,
            "solidity": result.solidity,
            "relative_contrast": result.relative_contrast,
            "axis_correlation": result.axis_correlation,
            "polarity": result.polarity,
            "sample_id": self.geometry_calibration_sample_id,
            "camera_serial_number": self.settings.get("camera_serial_number"),
            "exposure_ms": float(self.sidebar.exposure.value()),
            "updated_at": datetime.now().astimezone().isoformat(
                timespec="seconds"
            ),
            "verified_second_pass": False,
        }
        self.settings["geometry_calibration"] = profile
        try:
            self._persist_settings()
        except Exception as exc:
            QMessageBox.warning(
                self,
                "推掃取像參數校正（形變校正）",
                f"校正已套用，但無法儲存設定：\n{exc}",
            )

        self.geometry_calibration_active = False
        self.geometry_calibration_phase = None
        self.sidebar.geometry_calibration_button.setText("開始校正")
        self.sidebar.geometry_calibration_status.setText(
            f"已套用 line rate {applied_hz:.3f} Hz "
            f"（H/W {result.height_width_ratio:.3f}）"
        )
        self.live_buffer.clear()
        reset = getattr(self.stream_worker, "reset_analysis", None)
        if callable(reset):
            reset()
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(self.geometry_calibration_was_running)
        elif not self.geometry_calibration_was_running:
            self.stream_worker.pause()
        if not self.geometry_calibration_was_running:
            self.live_panel.set_message("檢測分析已停止")
        self._set_calibration_controls(True)
        self.geometry_calibration_current_line_rate_hz = None
        self.geometry_calibration_line_rate_min_hz = None
        self.geometry_calibration_line_rate_max_hz = None
        self.geometry_calibration_result = None
        self.geometry_calibration_sample_id = None
        QMessageBox.information(
            self,
            "推掃取像參數校正完成",
            f"偵測寬度：{result.width_px:.1f} px\n"
            f"偵測高度：{result.height_lines:.1f} lines\n"
            f"H/W：{result.height_width_ratio:.4f}\n"
            f"原 line rate：{previous_hz:.3f} Hz\n"
            f"新 line rate：{applied_hz:.3f} Hz",
        )

    def _fail_geometry_calibration(self, detail: str) -> None:
        was_running = self.geometry_calibration_was_running
        self.geometry_calibration_active = False
        self.geometry_calibration_phase = None
        self.geometry_calibration_current_line_rate_hz = None
        self.geometry_calibration_line_rate_min_hz = None
        self.geometry_calibration_line_rate_max_hz = None
        self.geometry_calibration_result = None
        self.geometry_calibration_sample_id = None
        reset = getattr(self.stream_worker, "reset_analysis", None)
        if callable(reset):
            reset()
        analysis_toggle = getattr(
            self.stream_worker,
            "set_analysis_enabled",
            None,
        )
        if callable(analysis_toggle):
            analysis_toggle(was_running)
        self.sidebar.geometry_calibration_button.setText("開始校正")
        self.sidebar.geometry_calibration_status.setText("校正失敗")
        self._set_calibration_controls(True)
        QMessageBox.critical(self, "推掃取像參數校正失敗", detail)

    def _handle_camera_connected(self, message: StreamMessage) -> None:
        model = (message.camera_model or "FX17").strip()
        serial = (message.camera_serial_number or "").strip()
        vendor = (message.camera_vendor or "").strip()
        lines = [f"已連線：{model}", f"序號：{serial or '未提供'}"]
        if vendor:
            lines.append(f"廠牌：{vendor}")
        if message.camera_auto_selected:
            lines.append("已自動選擇唯一的 FX17")

        previous_serial = str(
            self.settings.get("camera_serial_number") or ""
        ).strip()
        profile = self.settings.get("geometry_calibration", {})
        profile_serial = str(
            profile.get("camera_serial_number") or ""
        ).strip()
        if serial and profile.get("valid") and profile_serial != serial:
            profile["valid"] = False
            profile["invalid_reason"] = "相機序號已變更"
            self.sidebar.geometry_calibration_status.setText(
                "相機序號已變更，請重新進行推掃取像參數校正"
            )
        if serial:
            self.settings["camera_serial_number"] = serial
        self.settings["camera_model"] = model
        try:
            self._persist_settings()
        except Exception:
            lines.append("無法儲存相機識別設定")
        if previous_serial and serial and previous_serial != serial:
            lines.append(f"原設定序號：{previous_serial}")
        self.sidebar.camera_identity_status.setText("\n".join(lines))

    def _handle_line_rate_message(self, message: StreamMessage) -> None:
        if message.kind == "line_rate_report":
            if (
                not self.geometry_calibration_active
                or self.geometry_calibration_phase != "awaiting_line_rate"
            ):
                return
            if (
                message.line_rate_hz is None
                or message.line_rate_min_hz is None
                or message.line_rate_max_hz is None
            ):
                self._fail_geometry_calibration(
                    "FX17 未回傳有效的 line rate 或允許範圍。"
                )
                return
            self.geometry_calibration_current_line_rate_hz = float(
                message.line_rate_hz
            )
            self.geometry_calibration_line_rate_min_hz = float(
                message.line_rate_min_hz
            )
            self.geometry_calibration_line_rate_max_hz = float(
                message.line_rate_max_hz
            )
            self.geometry_calibration_phase = "capturing_standard"
            reset = getattr(self.stream_worker, "reset_analysis", None)
            if callable(reset):
                reset()
            analysis_toggle = getattr(
                self.stream_worker,
                "set_analysis_enabled",
                None,
            )
            if callable(analysis_toggle):
                analysis_toggle(True)
            self.live_panel.set_message("等待圓形標準件進入輸送帶視野…")
            self.sidebar.geometry_calibration_status.setText(
                f"目前 line rate {message.line_rate_hz:.3f} Hz；"
                "輸送帶定速後，請放入完整、平整的圓形標準件"
            )
            return
        if message.kind == "line_rate_set":
            self._finish_geometry_line_rate(message)
            return
        if message.kind == "line_rate_failed" and self.geometry_calibration_active:
            detail = message.error or "FX17 line rate 讀寫失敗。"
            self._fail_geometry_calibration(detail)

    def _set_camera_exposure(self, exposure_ms: float) -> None:
        exposure_us = float(exposure_ms) * 1000.0
        setter = getattr(self.stream_worker, "set_exposure_us", None)
        if callable(setter):
            setter(exposure_us)
        self.settings["exposure_ms"] = float(exposure_ms)
        self.settings["exposure_us"] = exposure_us
        profile = self.settings.get("geometry_calibration", {})
        calibrated_exposure = profile.get("exposure_ms")
        if (
            profile.get("valid")
            and calibrated_exposure is not None
            and abs(float(calibrated_exposure) - float(exposure_ms)) > 1e-6
        ):
            profile["valid"] = False
            profile["invalid_reason"] = "曝光時間已變更"
            self.sidebar.geometry_calibration_status.setText(
                "曝光時間已變更，請重新進行推掃取像參數校正"
            )
        self._persist_settings()

    def _set_calibration_controls(self, enabled: bool) -> None:
        capture_supported = callable(
            getattr(self.stream_worker, "capture_reference", None)
        )
        selection_supported = callable(
            getattr(self.stream_worker, "set_calibration_files", None)
        )
        line_rate_supported = callable(
            getattr(self.stream_worker, "request_line_rate", None)
        ) and callable(
            getattr(self.stream_worker, "set_line_rate_hz", None)
        )
        self.sidebar.capture_calibration_button.setEnabled(
            enabled and capture_supported
        )
        self.sidebar.select_calibration_files_button.setEnabled(
            enabled and selection_supported
        )
        self.sidebar.geometry_calibration_button.setEnabled(
            line_rate_supported
            and (enabled or self.geometry_calibration_active)
        )
        self.sidebar.exposure.setEnabled(enabled)
        self.sidebar.select_analysis_output_button.setEnabled(
            enabled and self.analysis_session is None
        )
        self.sidebar.start_button.setEnabled(
            enabled and not self.running and self.analysis_session is None
        )
        self.sidebar.stop_button.setEnabled(enabled and self.running)

    @staticmethod
    def _calibration_start_directory(path_text: str) -> Path:
        path = Path(path_text.strip()) if path_text.strip() else APP_DIR
        if path.is_file():
            return path.parent
        if path.is_dir():
            return path
        return APP_DIR

    def _select_analysis_output_directory(self) -> None:
        current_text = self.sidebar.analysis_output.line_edit.text().strip()  # type: ignore[attr-defined]
        try:
            start_directory = self._analysis_output_root()
        except Exception:
            start_directory = APP_DIR
        if not start_directory.is_dir():
            start_directory = start_directory.parent
        selected = QFileDialog.getExistingDirectory(
            self,
            "選擇分析結果儲存路徑",
            str(start_directory),
        )
        if not selected:
            return
        selected_path = str(Path(selected).resolve())
        self.sidebar.analysis_output.line_edit.setText(selected_path)  # type: ignore[attr-defined]
        self.settings["analysis_output_dir"] = selected_path
        try:
            self._persist_settings()
        except Exception as exc:
            QMessageBox.warning(
                self,
                "儲存路徑已選擇",
                f"本次會使用所選路徑，但無法寫入設定檔：\n{exc}",
            )

    def _select_calibration_files(self) -> None:
        setter = getattr(self.stream_worker, "set_calibration_files", None)
        if not callable(setter):
            QMessageBox.information(
                self,
                "選擇校正檔",
                "目前的串流來源不支援即時更換校正檔。",
            )
            return
        dark_current = self.sidebar.dark_ref.line_edit.text()  # type: ignore[attr-defined]
        dark_path, _ = QFileDialog.getOpenFileName(
            self,
            "選擇黑校正檔",
            str(self._calibration_start_directory(dark_current)),
            "Calibration data (*.dat *.raw);;All files (*.*)",
        )
        if not dark_path:
            return
        white_current = self.sidebar.white_ref.line_edit.text()  # type: ignore[attr-defined]
        white_directory = self._calibration_start_directory(white_current)
        if white_directory == APP_DIR:
            white_directory = Path(dark_path).parent
        white_path, _ = QFileDialog.getOpenFileName(
            self,
            "選擇白校正檔",
            str(white_directory),
            "Calibration data (*.dat *.raw);;All files (*.*)",
        )
        if not white_path:
            return
        self.calibration_file_selection_pending = True
        self._set_calibration_controls(False)
        self.detector_panel.set_message("正在驗證並載入黑／白校正檔…")
        setter(dark_path, white_path)

    def _finish_calibration_file_load(self, message: StreamMessage) -> None:
        self.calibration_file_selection_pending = False
        self._set_calibration_controls(True)
        if message.kind == "calibration_files_failed":
            detail = (
                message.error.strip().splitlines()[-1]
                if message.error
                else "黑／白校正檔無法形成有效配對"
            )
            self.detector_panel.set_message("黑／白校正檔載入失敗")
            QMessageBox.critical(
                self,
                "校正檔載入失敗",
                f"{detail}\n\n目前使用中的校正檔未變更。",
            )
            return
        if not message.dark_ref_path or not message.white_ref_path:
            return
        self.sidebar.dark_ref.line_edit.setText(message.dark_ref_path)  # type: ignore[attr-defined]
        self.sidebar.white_ref.line_edit.setText(message.white_ref_path)  # type: ignore[attr-defined]
        try:
            self._persist_calibration_paths()
        except Exception as exc:
            QMessageBox.warning(
                self,
                "校正檔已載入",
                f"pipeline 已更新，但無法儲存設定：\n{exc}",
            )
        self.live_buffer.clear()
        self.pending_cubes.clear()
        self.detector_panel.set_message("黑／白校正檔已載入")

    def _start_calibration_pair(self) -> None:
        capture = getattr(self.stream_worker, "capture_reference", None)
        if not callable(capture):
            QMessageBox.information(
                self,
                "校正取像",
                "目前的 recorded 來源不支援拍攝校正檔，請切換到 camera。",
            )
            return
        if self.calibration_in_progress is not None:
            return

        current_text = self.sidebar.dark_ref.line_edit.text().strip()  # type: ignore[attr-defined]
        current_path = Path(current_text) if current_text else APP_DIR
        directory = (
            current_path.parent
            if current_path.suffix
            else current_path
        )
        if not directory.is_dir():
            directory = APP_DIR
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        serial = str(
            self.settings.get("camera_serial_number") or "unknown"
        ).strip()
        serial = "".join(
            "-" if character in '<>:"/\\|?*' else character
            for character in serial
        ).strip(". ") or "unknown"
        exposure = self.sidebar.exposure.value()
        suggested_name = f"FX17_{serial}_{exposure:g}ms_{timestamp}.dat"
        selected_path, _ = QFileDialog.getSaveFileName(
            self,
            "設定黑／白校正檔共用名稱",
            str(directory / suggested_name),
            "Calibration data (*.dat);;Raw data (*.raw);;All files (*.*)",
        )
        if not selected_path:
            return
        try:
            pair_paths = calibration_pair_paths(selected_path)
        except ValueError as exc:
            QMessageBox.warning(self, "校正檔名無效", str(exc))
            return
        existing = [path for path in pair_paths.values() if path.exists()]
        if existing:
            overwrite = QMessageBox.question(
                self,
                "校正檔已存在",
                "下列校正檔已存在：\n"
                + "\n".join(path.name for path in existing)
                + "\n\n是否覆寫？",
                QMessageBox.StandardButton.Yes
                | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if overwrite != QMessageBox.StandardButton.Yes:
                return
        answer = QMessageBox.question(
            self,
            "準備黑校正",
            f"黑校正檔：{pair_paths['dark'].name}\n"
            f"白校正檔：{pair_paths['white'].name}\n\n"
            "即將自動關閉快門並拍攝黑校正。\n"
            "黑校正完成後，程式會再提示放置白參考板。",
            QMessageBox.StandardButton.Ok
            | QMessageBox.StandardButton.Cancel,
            QMessageBox.StandardButton.Ok,
        )
        if answer != QMessageBox.StandardButton.Ok:
            return
        self.calibration_pair_paths = pair_paths
        try:
            capture(
                "dark",
                self.calibration_pair_paths["dark"],
                activate_after_capture=False,
            )
        except Exception as exc:
            self.calibration_pair_paths = None
            QMessageBox.critical(self, "校正取像失敗", str(exc))
            return
        self.calibration_in_progress = "dark"
        self._set_calibration_controls(False)
        self.detector_panel.set_message("正在拍攝黑校正…")

    def _start_white_pair_capture(self) -> None:
        if self.calibration_pair_paths is None:
            self.calibration_in_progress = None
            self._set_calibration_controls(True)
            return
        answer = QMessageBox.question(
            self,
            "準備白校正",
            "黑校正已完成。\n\n"
            "請放置均勻白參考板，保持照明與曝光不變，再開始白校正。",
            QMessageBox.StandardButton.Ok
            | QMessageBox.StandardButton.Cancel,
            QMessageBox.StandardButton.Ok,
        )
        if answer != QMessageBox.StandardButton.Ok:
            self.calibration_pair_paths = None
            self.calibration_in_progress = None
            self._set_calibration_controls(True)
            self.detector_panel.set_message("黑校正已儲存；校正配對尚未完成")
            return
        capture = getattr(self.stream_worker, "capture_reference", None)
        try:
            capture(
                "white",
                self.calibration_pair_paths["white"],
                activate_after_capture=True,
            )
        except Exception as exc:
            self.calibration_pair_paths = None
            self.calibration_in_progress = None
            self._set_calibration_controls(True)
            QMessageBox.critical(self, "校正取像失敗", str(exc))
            return
        self.calibration_in_progress = "white"
        self.detector_panel.set_message("正在拍攝白校正…")

    def _persist_settings(self) -> None:
        temporary = SETTINGS_PATH.with_suffix(".json.tmp")
        try:
            temporary.write_text(
                json.dumps(self.settings, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            temporary.replace(SETTINGS_PATH)
        finally:
            temporary.unlink(missing_ok=True)

    def _persist_calibration_paths(self) -> None:
        self.settings["dark_ref_path"] = (
            self.sidebar.dark_ref.line_edit.text().strip()  # type: ignore[attr-defined]
        )
        self.settings["white_ref_path"] = (
            self.sidebar.white_ref.line_edit.text().strip()  # type: ignore[attr-defined]
        )
        self._persist_settings()

    def _finish_reference_capture(self, message: StreamMessage) -> None:
        kind = message.calibration_kind
        if kind not in ("dark", "white"):
            return
        if message.kind == "calibration_failed":
            self.calibration_in_progress = None
            self.calibration_pair_paths = None
            self._set_calibration_controls(True)
            self.detector_panel.set_message("校正取像失敗")
            QMessageBox.critical(
                self,
                "校正取像失敗",
                (
                    message.error.strip().splitlines()[-1]
                    if message.error
                    else "未知錯誤"
                ),
            )
            return
        if not message.calibration_path:
            return
        row = self.sidebar.dark_ref if kind == "dark" else self.sidebar.white_ref
        row.line_edit.setText(  # type: ignore[attr-defined]
            message.calibration_path
        )
        label = "黑校正" if kind == "dark" else "白校正"
        if kind == "dark" and self.calibration_pair_paths is not None:
            self.calibration_in_progress = None
            self.detector_panel.set_message("黑校正完成；等待白校正")
            self._start_white_pair_capture()
            return
        if message.calibration_activated is False:
            detail = (
                message.error.strip().splitlines()[-1]
                if message.error
                else "黑白參考目前無法形成有效配對"
            )
            self.detector_panel.set_message(f"{label}已儲存，尚未啟用")
            QMessageBox.warning(
                self,
                "校正檔已儲存",
                f"{label}檔已儲存，但目前尚未啟用。\n"
                f"{detail}\n\n請用相同曝光拍攝另一份校正檔。",
            )
            self.calibration_in_progress = None
            self.calibration_pair_paths = None
            self._set_calibration_controls(True)
            return
        try:
            self._persist_calibration_paths()
        except Exception as exc:
            QMessageBox.warning(
                self,
                "校正完成",
                f"校正檔已儲存，但無法更新設定檔：\n{exc}",
            )
        self.detector_panel.set_message(
            f"黑／白校正完成（各 {message.calibration_frame_count} 幀）"
        )
        self.calibration_in_progress = None
        self.calibration_pair_paths = None
        self._set_calibration_controls(True)

    def _tick(self) -> None:
        self._poll_stream_worker()
        self._poll_analysis_worker()
        self._submit_pending_cubes()

    def _poll_stream_worker(self) -> None:
        for message in self.stream_worker.poll():
            if message.kind == "camera_connected":
                self._handle_camera_connected(message)
            elif message.kind == "preview" and (
                message.detector_frame is not None
                or (
                    (self.running or self.geometry_calibration_active)
                    and message.display_lines is not None
                )
            ):
                self._show_stream_preview(message)
            elif message.kind == "sample_start":
                if (
                    self.geometry_calibration_active
                    and self.geometry_calibration_phase == "capturing_standard"
                ):
                    self.sidebar.geometry_calibration_status.setText(
                        "已偵測到標準件，正在等待它完整離開視野…"
                    )
                elif self.running:
                    self.current_sample_id = message.sample_id
                    if message.sample_id is not None:
                        self.sample_started_at[message.sample_id] = datetime.now()
            elif (
                message.kind == "sample_end"
                and message.cube is not None
            ):
                if self.geometry_calibration_active:
                    self._accept_geometry_calibration(message)
                elif self.running:
                    self._queue_detected_cube(message)
            elif (
                message.kind == "sample_rejected"
                and self.geometry_calibration_active
                and self.geometry_calibration_phase == "capturing_standard"
            ):
                self.sidebar.geometry_calibration_status.setText(
                    "標準件影像不完整或太短，請重新放入一次"
                )
            elif message.kind == "calibration_started":
                self.detector_panel.set_message(
                    "正在拍攝黑校正…"
                    if message.calibration_kind == "dark"
                    else "正在拍攝白校正…"
                )
            elif message.kind in (
                "calibration_complete",
                "calibration_failed",
            ):
                self._finish_reference_capture(message)
            elif message.kind in (
                "calibration_files_loaded",
                "calibration_files_failed",
            ):
                self._finish_calibration_file_load(message)
            elif message.kind in (
                "line_rate_report",
                "line_rate_set",
                "line_rate_failed",
            ):
                self._handle_line_rate_message(message)
            elif message.kind == "error":
                if str(self.settings.get("stream_source", "")).lower() == "camera":
                    self.sidebar.camera_identity_status.setText("相機：連線失敗")
                if self.calibration_in_progress is not None:
                    self.calibration_in_progress = None
                    self.calibration_pair_paths = None
                    self._set_calibration_controls(True)
                if self.geometry_calibration_active:
                    self.geometry_calibration_active = False
                    self.sidebar.geometry_calibration_button.setText(
                        "開始校正"
                    )
                    self.sidebar.geometry_calibration_status.setText(
                        "推掃取像參數校正因串流錯誤而中止"
                    )
                    self._set_calibration_controls(True)
                self.detector_panel.set_title("FX17 取像失敗")
                self.detector_panel.set_message(
                    stream_error_message(message.error)
                )
                self.live_panel.set_message("檢測分析無法啟動")

    def _show_stream_preview(self, message: StreamMessage) -> None:
        if (
            (self.running or self.geometry_calibration_active)
            and message.display_lines is not None
        ):
            self.live_buffer.push(message.display_lines)
            self.live_panel.set_image(self.live_buffer.frame())
        if message.detector_frame is not None:
            bands, width = message.detector_frame.shape[:2]
            score_text = (
                ""
                if message.material_score is None
                else f" · material {message.material_score:.3f}"
            )
            self.detector_panel.set_title(
                f"最新原始幀 · {bands} bands × {width} px{score_text}"
            )
            self.detector_panel.set_image(
                message.detector_frame,
                value_range=message.detector_value_range,
                spectral_range_nm=message.detector_spectral_range_nm,
            )
        if message.false_color_wavelengths_nm is not None:
            red, green, blue = message.false_color_wavelengths_nm
            self.live_panel.eyebrow_label.setText(
                f"即時偽色影像 · R {red:.0f} / G {green:.0f} / B {blue:.0f} nm"
            )
        if message.sample_id is not None:
            if (
                message.sample_index is not None
                and message.sample_count is not None
            ):
                index = message.sample_index + 1
                self.live_panel.set_title(
                    f"{message.sample_id}  ({index}/{message.sample_count})"
                )
            else:
                self.live_panel.set_title(message.sample_id)

    def _queue_detected_cube(self, message: StreamMessage) -> None:
        sample_id = message.sample_id or "detected_sample"
        if self.analysis_failed:
            self._show_analysis_error(sample_id)
            return
        capture_ended_at = datetime.now()
        capture_started_at = self.sample_started_at.pop(
            sample_id,
            capture_ended_at,
        )
        acquisition_metadata = self._sample_record_parameters(message)
        acquisition_metadata["capture_timing"] = {
            "started_at": capture_started_at.isoformat(timespec="microseconds"),
            "ended_at": capture_ended_at.isoformat(timespec="microseconds"),
            "file_timestamp": capture_started_at.strftime("%Y%m%d_%H%M%S_%f"),
        }
        output_target = None
        if self.analysis_session is not None:
            try:
                output_target = self.analysis_session.register_sample(
                    sample_id=sample_id,
                    capture_started_at=capture_started_at,
                    capture_ended_at=capture_ended_at,
                    acquisition_metadata=acquisition_metadata,
                )
            except Exception as exc:
                QMessageBox.critical(
                    self,
                    "樣本儲存失敗",
                    f"無法建立樣本資料夾或Excel紀錄：\n{exc}",
                )
                self._show_analysis_error(sample_id)
                self.stop_stream()
                return
            self.session_outstanding_records.add(output_target.record_id)
        self.pending_cubes.append(
            (
                sample_id,
                message.cube,
                message.raw_cube,
                acquisition_metadata,
                output_target,
            )
        )

    def _sample_record_parameters(self, message: StreamMessage) -> dict:
        """Snapshot acquisition and processing parameters at sample completion."""
        geometry = self.settings.get("geometry_calibration", {})
        line_rate_hz = (
            geometry.get("applied_line_rate_hz")
            if isinstance(geometry, dict)
            else None
        )
        settings_snapshot = json.loads(
            json.dumps(self.settings, ensure_ascii=False, default=str)
        )
        return {
            "stream_source": str(self.settings.get("stream_source", "camera")),
            "sample_boundary": {
                "sample_line_count": message.sample_line_count,
                "end_material_score": message.material_score,
                "start_threshold": float(
                    self.settings.get("sample_start_threshold", 0.05)
                ),
                "end_threshold": float(
                    self.settings.get("sample_end_threshold", 0.04)
                ),
                "start_confirm_lines": int(
                    self.settings.get("start_confirm_lines", 4)
                ),
                "end_confirm_lines": int(
                    self.settings.get("end_confirm_lines", 8)
                ),
                "min_sample_lines": int(
                    self.settings.get("min_sample_lines", 200)
                ),
                "max_sample_lines": int(
                    self.settings.get("max_sample_lines", 1200)
                ),
            },
            "camera": {
                "model": self.settings.get("camera_model"),
                "serial_number": self.settings.get("camera_serial_number"),
                "device_index": int(self.settings.get("camera_device_index", 0)),
                "source_bands": int(self.settings.get("camera_bands", 224)),
                "spatial_width": int(self.settings.get("camera_width", 640)),
                "pixel_format": "Mono12Packed",
                "cti_path": str(
                    resolve_setting_path(
                        self.settings.get(
                            "cti_path",
                            "../fx_test/mvGenTLProducer.cti",
                        )
                    )
                ),
            },
            "acquisition": {
                "exposure_ms": float(self.settings.get("exposure_ms", 3.0)),
                "exposure_us": exposure_us_from_settings(self.settings),
                "line_rate_hz": (
                    None if line_rate_hz is None else float(line_rate_hz)
                ),
                "integrity": message.acquisition_integrity,
            },
            "black_white_calibration": {
                "dark_ref_path": str(
                    resolve_setting_path(self.settings["dark_ref_path"])
                ),
                "white_ref_path": str(
                    resolve_setting_path(self.settings["white_ref_path"])
                ),
                "trim_bands": int(self.settings.get("trim_bands", 10)),
                "clip_max": float(self.settings.get("clip_max", 1.2)),
            },
            "spatial_roi": {
                "start": int(self.settings.get("spatial_roi_start", 0)),
                "end": self.settings.get("spatial_roi_end"),
            },
            "material_pixel_threshold": float(
                self.settings.get("material_pixel_threshold", 0.08)
            ),
            "analysis_device": str(
                self.settings.get("analysis_device", "cuda")
            ),
            "geometry_calibration": geometry,
            "settings_snapshot": settings_snapshot,
        }

    def _submit_pending_cubes(self) -> None:
        if self.analysis_failed:
            self.pending_cubes.clear()
            return
        while self.pending_cubes:
            (
                sample_id,
                cube,
                raw_cube,
                acquisition_metadata,
                output_target,
            ) = self.pending_cubes[0]
            if not self.analysis_worker.submit_cube(
                sample_id,
                cube,
                raw_cube=raw_cube,
                acquisition_metadata=acquisition_metadata,
                output_target=output_target,
            ):
                return
            self.pending_cubes.pop(0)

    def _poll_analysis_worker(self) -> None:
        for message in self.analysis_worker.poll():
            if message.kind == "result" and message.result is not None:
                if self.geometry_calibration_active:
                    continue
                result = message.result
                if self.analysis_session is not None and result.record_id:
                    try:
                        self.analysis_session.complete_sample(
                            result.record_id,
                            uniformity=result.uniformity,
                            resistance_ohm=result.resistance_ohm,
                            sample_lines=result.sample_lines,
                            elapsed_seconds=result.elapsed_seconds,
                        )
                    except Exception as exc:
                        QMessageBox.warning(
                            self,
                            "Excel更新失敗",
                            f"樣本檔案已保存，但Excel無法更新：\n{exc}",
                        )
                    self.session_outstanding_records.discard(result.record_id)
                self._show_live_result(message.result)
                self._maybe_finalize_analysis_session()
                continue
            if message.kind == "error":
                if message.sample_id is None:
                    self.analysis_failed = True
                    if self.analysis_session is not None:
                        for item in self.pending_cubes:
                            output_target = item[4]
                            if output_target is not None:
                                self.analysis_session.fail_sample(
                                    output_target.record_id,
                                    message.error or "模型初始化失敗",
                                )
                                self.session_outstanding_records.discard(
                                    output_target.record_id
                                )
                    self.pending_cubes.clear()
                else:
                    output_target = message.output_target
                    if self.analysis_session is not None and output_target is not None:
                        try:
                            self.analysis_session.fail_sample(
                                output_target.record_id,
                                message.error or "分析失敗",
                            )
                        except Exception as exc:
                            QMessageBox.warning(
                                self,
                                "Excel更新失敗",
                                f"無法把分析失敗狀態寫入Excel：\n{exc}",
                            )
                        self.session_outstanding_records.discard(
                            output_target.record_id
                        )
                    self._show_analysis_error(message.sample_id)
                self._maybe_finalize_analysis_session()

    def _show_live_result(self, result: LiveAnalysisResult) -> None:
        self.last_result_sample_id = result.sample_id
        self.result_panel.set_title(f"均勻度結果 · {result.sample_id}")
        eyebrow = (
            "分析結果 · 未校正／已校正 RAW、HDR、參數已保存"
            if result.sample_record_path
            else (
                "分析結果 · 已保存診斷 cube"
                if result.capture_path
                else "分析結果"
            )
        )
        if result.quality_warnings:
            eyebrow += " · " + "；".join(result.quality_warnings)
        self.result_panel.eyebrow_label.setText(eyebrow)
        self.resistance_card.set_value(result.resistance_ohm, decimals=2)
        self.uniformity_card.set_value(result.uniformity, decimals=1)
        self.result_panel.set_image(result.heatmap)

    def _show_analysis_error(self, sample_id: str) -> None:
        self.result_panel.set_title(f"均勻度結果 · {sample_id}")
        self.uniformity_card.set_value(None)
        self.resistance_card.set_value(None)
        self.result_panel.set_message("即時模型分析無法完成")

    def closeEvent(self, event) -> None:  # noqa: N802
        if self.analysis_session is not None:
            try:
                self.analysis_session.finish(
                    datetime.now(),
                    incomplete_note="程式關閉前分析尚未完成",
                )
            except Exception:
                pass
        self.stream_worker.close()
        self.analysis_worker.close()
        super().closeEvent(event)


def apply_style(app: QApplication) -> None:
    app.setStyleSheet(
        """
        QWidget {
            background: #242424;
            color: #e4e4e4;
            font-family: "Microsoft JhengHei", "Noto Sans TC", "Segoe UI";
            font-size: 13px;
        }
        #eyebrow, #metricTitle, #fieldLabel {
            color: #b7b7b7;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton {
            min-height: 27px;
            padding: 0 10px;
            border-radius: 2px;
            font-weight: 800;
        }
        #primaryButton {
            color: #f2f2f2;
            background: #3b5f78;
            border: 1px solid #7892a4;
        }
        #primaryButton:hover { background: #4a738f; }
        #secondaryButton, #smallButton {
            color: #eeeeee;
            background: #333333;
            border: 1px solid #6f6f6f;
        }
        #smallButton {
            min-height: 25px;
            padding: 0 8px;
        }
        #sidebar {
            background: #1b1b1b;
            border: 1px solid #777777;
            border-radius: 2px;
        }
        #controlGroup {
            background: #202020;
            border: 1px solid #696969;
            border-radius: 2px;
        }
        #groupTitle {
            color: #f0f0f0;
            font-size: 13px;
            font-weight: 900;
        }
        #panel, #metricCard {
            background: #2b2b2b;
            border: 1px solid #777777;
            border-radius: 2px;
        }
        #panelTitle {
            color: #f0f0f0;
            font-size: 18px;
            font-weight: 800;
        }
        #imageViewport {
            background: #111111;
            border: 1px solid #7a7a7a;
            border-radius: 1px;
            color: #d6b36a;
            font-size: 16px;
            font-weight: 700;
        }
        QLineEdit, QSpinBox {
            min-height: 26px;
            padding: 2px 6px;
            color: #f0f0f0;
            background: #171717;
            border: 1px solid #686868;
            border-radius: 2px;
            selection-background-color: #3b5f78;
        }
        #metricValue {
            color: #f4f4f4;
            font-size: 48px;
            font-weight: 900;
        }
        #metricUnit {
            color: #b9b9b9;
            font-size: 18px;
            font-weight: 800;
            padding-bottom: 7px;
        }
        """
    )


def main() -> int:
    app = QApplication(sys.argv)
    apply_style(app)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
