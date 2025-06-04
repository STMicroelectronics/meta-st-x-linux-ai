from typing import List, Tuple, Sequence
from numpy.typing import NDArray
import numpy as np
from pathlib import Path
from stai_mpu import _binding
from enum import Enum

class stai_mpu_backend_engine(Enum):
    STAI_MPU_TFLITE_CPU_ENGINE = 0
    STAI_MPU_TFLITE_NPU_ENGINE = 1
    STAI_MPU_ORT_CPU_ENGINE = 2
    STAI_MPU_ORT_NPU_ENGINE = 3
    STAI_MPU_OVX_NPU_ENGINE = 4

class stai_mpu_tensor:
    name: str = ...
    index: int = ...
    rank: int = ...
    shape: Tuple[int, ...] = ...
    dtype: str = ...
    qtype: str = ...
    scale: float = ...
    zero_point: int = ...
    fixed_point_pos: int = ...

class stai_mpu_network:
    def __init__(self, model_path: Path, use_hw_acceleration: bool = True) -> None:
        self._exec = _binding.stai_mpu_network(model_path, use_hw_acceleration)

    def get_num_inputs(self) -> int:
        return self._exec.get_num_inputs()

    def get_num_outputs(self) -> int:
        return self._exec.get_num_outputs()

    def get_backend_engine(self) -> stai_mpu_backend_engine:
        return self._exec.get_backend_engine()

    def get_input_infos(self) -> List[stai_mpu_tensor]:
        return self._exec.get_input_infos()

    def get_output_infos(self) -> List[stai_mpu_tensor]:
        return self._exec.get_output_infos()

    def set_input(self, index: int, input_tensor: NDArray) -> None:
        return self._exec.set_input(index, input_tensor)

    def get_output(self, index: int) -> NDArray:
        output_tensor: NDArray = self._exec.get_output(index)
        return output_tensor

    def set_inputs(self, input_tensors: Sequence[NDArray]) -> None:
        for i, tensor in enumerate(input_tensors):
            self.set_input(i, tensor)

    def get_outputs(self) -> List[NDArray]:
        output_tensors: List[NDArray] = []
        num_outputs = self.get_num_outputs()
        for i in range(num_outputs):
            output_tensor = self.get_output(i)
            output_tensors.append(output_tensor)

        return output_tensors

    def run(self) -> None:
        self._exec.run()
