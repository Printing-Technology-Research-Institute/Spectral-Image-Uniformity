# -*- coding: utf-8 -*-
"""
GenICam device control interface

requirement:
harvesters==1.3.2
numpy

"""
import os

import numpy as np
from genicam import genapi
from harvesters.core import Harvester
from logging import getLogger

PixelFormat = dict(Mono12Packed=4095)


class Camera:
    def __init__(self):
        """
        初始化連接裝置用的函式庫與 cti 檔案。
        """
        self._h = Harvester(logger=getLogger())
        self._h.add_file(os.path.dirname(__file__) + r"\mvGenTLProducer.cti")
        self._current_exposure = 0

    def _init_feature(self):
        """
        掃描裝置必要控制參數。
        """
        node_map = self.ia.remote_device.node_map
        self.features = {
            "Specim": self._gen_feature(node_map.Specim),
            "AcquisitionControl": self._gen_feature(node_map.AcquisitionControl),
            "ImageFormatControl": self._gen_feature(node_map.ImageFormatControl),
        }

    def _default_setting(self):
        # 關閉 EnableAcquisitionFrameRate
        self.features["AcquisitionControl"]["EnAcquisitionFrameRate"].value = False
        self.features["ImageFormatControl"]["PixelFormat"].value = "Mono12Packed"

    def _gen_feature(self, feature_root):
        """
        掃描裝置必要控制參數。
        """
        return {
            i.node.name: (
                {ii.node.name: ii for ii in i.features}
                if genapi.EInterfaceType.intfICategory
                == i.node.principal_interface_type
                else i
            )
            for i in feature_root.features
        }

    def _connect(self, index):
        """
        透過 harvesters 連接裝置，並掃描必要控制參數與初始設定。
        """
        print("Connect to cam")
        self.ia = self._h.create(index)

        print("Init feature")
        self._init_feature()

        print("Set default setting")
        self._default_setting()

    """
    Camera control
    """

    def get_device_list(self):
        """
        取得當前可用裝置清單。
        """
        self._h.update()
        return self._h.device_info_list

    def connect_first(self):
        """
        連接第一台裝置。
        """
        device_list = self.get_device_list()
        print(device_list[0].model)
        self._connect(0)

    def connect_by_index(self, index):
        """
        連接第 index 台裝置。
        """
        self._connect(index)

    def start(self):
        """
        裝置開始串流。
        """
        self.ia.start()

    def stop(self):
        """
        裝置停止串流。
        """
        self.ia.stop()

    def disconnect(self):
        """
        關閉快門並斷開連接。
        """
        self.shutter_off()
        self.ia.destroy()
        self._h.reset()

    """
    Camera setting
    """

    def get_width(self):
        """
        取得 Sample 數量。

        return: int
        """
        return int(self.features["ImageFormatControl"]["Width"].value)

    def get_height(self):
        """
        取得 Bands 數量。
        
        return: int
        """
        return int(self.features["ImageFormatControl"]["Height"].value)

    def get_shape(self):
        """
        取得影像大小。
        
        return: tuple(int, int)
        """
        return (self.get_height(), self.get_width())

    def get_max_count(self):
        """
        取得位元數。

        return: int
        """
        _pixelformat = self.features["ImageFormatControl"]["PixelFormat"].value
        return PixelFormat[_pixelformat]

    def get_exposure(self):
        """
        取得曝光時間 (ms)。

        return: float
        """
        self._current_exposure = (
            self.features["AcquisitionControl"]["ExposureTime"].value / 1000
        )
        return self._current_exposure

    def get_data(self):
        """
        取得影像。

        return: np.ndarray
        """
        buffer = self.ia.fetch(timeout=0.5)
        comp = buffer.payload.components[0]
        self.data = comp.data.reshape(comp.height, comp.width)
        if buffer:
            buffer.queue()
        return np.asarray(self.data, dtype="f")

    """
    Camera control
    """

    def shutter_on(self):
        """
        快門開啟
        """
        self.features["Specim"]["MotorShutter"]["MotorShutter_PulseRev"].value = 190
        self.features["Specim"]["MotorShutter"]["MotorShutter_PulseRev"].value = 200

    def shutter_off(self):
        """
        快門關閉
        """
        self.features["Specim"]["MotorShutter"]["MotorShutter_PulseFwd"].value = 190
        self.features["Specim"]["MotorShutter"]["MotorShutter_PulseFwd"].value = 200

    def set_exposure(self, ex_ms):
        """
        設定曝光時間 (ms)。
        """
        self._current_exposure = ex_ms
        self.features["AcquisitionControl"]["ExposureTime"].value = ex_ms * 1000
