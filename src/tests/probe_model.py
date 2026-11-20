'''Create small CPU models and model files for regression tests without a trained model.'''

import json
from pathlib import Path

import torch

from compile import save_compiled_model
from deepshogi.config import (MODEL_INFO_OFFSET, MODEL_INPUT_SIZE, MODEL_OUTPUT_SIZE,
                              MODEL_VALUE_OFFSET)


class ProbeModel(torch.nn.Module):
    '''Expose input features and predictable values to check the native input layout.'''

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map input features to output features and game values.
        Args:
            inputs (torch.Tensor): Batched model input features.
        Returns:
            torch.Tensor: Batched policy, game value, and auxiliary outputs.
        '''
        # Echo all input features so native unpacking errors remain visible.
        policy = torch.cat((inputs, inputs), dim=1)[:, :MODEL_VALUE_OFFSET]
        info = inputs[:, MODEL_INFO_OFFSET:]
        value = torch.sigmoid(info[:, 77:78] + info[:, 81:82])
        remaining = info[:, 81:82] * 0.1 + 0.25
        return torch.cat((policy, value, value * 0, remaining, info[:, :6]), dim=1)


class SearchModel(torch.nn.Module):
    '''Provide deterministic CPU search outputs with uniform policies.'''

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map input features to policy and game value outputs.
        Args:
            inputs (torch.Tensor): Batched model input features.
        Returns:
            torch.Tensor: Batched policy, game value, and auxiliary outputs.
        '''
        # Match the native output layout using a single input feature.
        value = torch.sigmoid(inputs[:, :1])
        policy = value.expand(-1, MODEL_VALUE_OFFSET)
        return torch.cat((policy, value, value * 0, value * 0 + 1, inputs[:, :6]), dim=1)


class EchoModel(torch.nn.Module):
    '''Repeat scaled input features as outputs to compare compiled and original results.'''

    def __init__(self) -> None:
        '''Create the model with a weight so that precision changes also convert parameters.'''
        super().__init__()
        self.scale = torch.nn.Parameter(torch.tensor([0.5, 2.0]))

    def forward(self, inputs: torch.Tensor) -> torch.Tensor:
        '''Map input features to model outputs.
        Args:
            inputs (torch.Tensor): Batched model input features.
        Returns:
            torch.Tensor: Batched outputs with the model output width.
        '''
        # Repeat the scaled input features so the output width matches the model output size.
        outputs = torch.cat((inputs * self.scale[0], inputs * self.scale[1]), dim=1)
        return outputs[:, :MODEL_OUTPUT_SIZE]


def save_exported_probe(path: Path, model: torch.nn.Module, batch_size: int,
                        dynamic: bool = False, model_name: str | None = None) -> None:
    '''Export a probe model as a CPU FP32 ExportedProgram.
    Args:
        path (Path): Destination of the ExportedProgram.
        model (torch.nn.Module): Model mapping unpacked inputs to model outputs.
        batch_size (int): Batch size of the example input stored in the program.
        dynamic (bool): True to export the batch dimension as dynamic.
        model_name (str | None): Model name stored in the metadata, or None to omit metadata.
    '''
    # Store an example input; a dynamic batch requires an example batch of at least two.
    inputs = torch.zeros((batch_size, MODEL_INPUT_SIZE), dtype=torch.float32)
    dynamic_shapes = ({0: torch.export.Dim('batch', min=1)},) if dynamic else None
    exported = torch.export.export(
        model.eval(), (inputs,), dynamic_shapes=dynamic_shapes, strict=False)

    # Record the export conditions in the same format as the upstream exporter.
    extra_files: dict[str, str] = {}
    if model_name is not None:
        metadata: dict[str, str | int] = {
            'schema_version': 2 if dynamic else 1, 'model_name': model_name,
            'device_type': 'cpu', 'input_dtype': 'float32', 'precision': 'fp32'}
        if not dynamic:
            metadata['batch_size'] = batch_size
        extra_files['metadata.json'] = json.dumps(metadata)

    # Save through a file object so that the .ep extension does not trigger a warning.
    with path.open('wb') as stream:
        torch.export.save(exported, stream, extra_files=extra_files)


def save_compiled_probe(directory: Path, name: str, model: torch.nn.Module,
                        batch_size: int) -> Path:
    '''Compile a probe model into a CPU AOTInductor PT2 package.
    Args:
        directory (Path): Directory that keeps the intermediate and compiled files.
        name (str): Base name of the generated files.
        model (torch.nn.Module): Model mapping unpacked inputs to model outputs.
        batch_size (int): Fixed batch size of the compiled package.
    Returns:
        Path: Path of the compiled PT2 package.
    '''
    # Export with the same batch so that compilation keeps the stored graph.
    exported_path = directory / f'{name}.ep'
    compiled_path = directory / f'{name}.pt2'
    save_exported_probe(exported_path, model, batch_size)
    save_compiled_model(
        exported_path, compiled_path, backend='aoti', gpu=-1, batch_size=batch_size)

    return compiled_path
