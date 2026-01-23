"""
GNU Radio Out-of-Tree Module: gr-opus
Opus audio codec wrapper module
"""

try:
    from ._gr_opus_swig import opus_decoder, opus_encoder
except ImportError:
    try:
        from .gr_opus_swig import opus_decoder, opus_encoder
    except ImportError:
        try:
            from .opus_decoder import opus_decoder
            from .opus_encoder import opus_encoder
        except ImportError:
            import os

            dirname, filename = os.path.split(os.path.abspath(__file__))
            __path__.append(os.path.join(dirname, "bindings"))
            from .opus_decoder import opus_decoder
            from .opus_encoder import opus_encoder

__all__ = ["opus_encoder", "opus_decoder"]
