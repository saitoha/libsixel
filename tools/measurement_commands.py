"""Normalize recorded encoder arguments for measurement validation."""

_ARCHIVED_COLORSPACE_NAMES = {
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


_COLORSPACE_FIELDS = frozenset((
    "colorspace", "colorspaces", "working_colorspace", "clustering_colorspace",
    "output_colorspace", "loader_cms_target", "channel_l_colorspaces",
))


def recorded_colorspace_fields(record, is_colorspace=False):
    """Read legacy measurement fields without changing the archived data."""
    if isinstance(record, dict):
        converted = {
            key: recorded_colorspace_fields(value, key in _COLORSPACE_FIELDS)
            for key, value in record.items()
        }
        if "path_cases" in record and isinstance(record["path_cases"], list):
            converted["path_cases"] = [
                case[:3] + recorded_colorspace_fields(case[3:5], True) + case[5:]
                if isinstance(case, list) else case
                for case in record["path_cases"]
            ]
        if is_colorspace and "name" in converted:
            converted["name"] = recorded_colorspace_fields(converted["name"], True)
        if record.get("domain") == "clustering-colorspace" and "config" in record:
            converted["config"] = recorded_colorspace_fields(record["config"], True)
            if "option" in record:
                converted["option"] = canonical_encoder_arguments([record["option"]])[0]
        return converted
    if isinstance(record, list):
        return [recorded_colorspace_fields(value, is_colorspace)
                for value in record]
    if is_colorspace and isinstance(record, str):
        return _ARCHIVED_COLORSPACE_NAMES.get(record, record)
    return record


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
        normalized.append(option + _ARCHIVED_COLORSPACE_NAMES.get(name, name)
                          + colon + suffix)
    return normalized


def encoder_precision_matches(arguments, precision):
    """Require one request for the recorded precision, without overrides."""
    requests = [arg for arg in arguments if arg.startswith("--precision-policy=")]
    return requests == [f"--precision-policy={precision}"]
