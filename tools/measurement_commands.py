"""Normalize recorded encoder arguments for measurement validation."""

COLORSPACE_NAMES = {
    "gamma": "srgb",
    "linear": "srgb-linear",
    "cielab": "lab-d65",
    "adobe-rgb": "a98-rgb",
}
COLORSPACE_OPTIONS = {
    "--clustering-colorspace": "-X",
    "--working-colorspace": "-W",
    "--output-colorspace": "-U",
}


def canonical_encoder_arguments(arguments):
    """Compare spellings without editing the saved command or executing it."""
    normalized = []
    remaining = iter(arguments)
    for argument in remaining:
        if argument == "--":
            normalized.extend((argument, *remaining))
            break
        option, separator, value = argument.partition("=")
        if option in COLORSPACE_OPTIONS:
            option = COLORSPACE_OPTIONS[option]
            if not separator:
                value = next(remaining, "")
        elif argument.startswith(("-W", "-X", "-U")):
            option, value = argument[:2], argument[2:] or next(remaining, "")
        else:
            if option == "--precision" and separator:
                argument = "--precision-policy=" + value
            normalized.append(argument)
            continue
        name, colon, suffix = value.partition(":")
        normalized.append(option + COLORSPACE_NAMES.get(name, name)
                          + colon + suffix)
    return normalized
