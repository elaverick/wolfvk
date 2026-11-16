#include "pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
    printf(
        "Usage: %s --vswap <VSWAP.WL6> [--output <directory>] [options]\n"
        "\n"
        "Extracts every WL6 sprite, rebuilds it as a 64x64x64 MagicaVoxel model,\n"
        "writes one animated VOX per original animation and validates each model\n"
        "against the original frames.\n"
        "\n"
        "Options:\n"
        "  --output <dir>             Output directory (default: sprite_voxels)\n"
        "  --only <model>             Rebuild one model, e.g. grd_s or stat_1\n"
        "  --carve-misses <n>         Views allowed to disagree before carving (default 0)\n"
        "  --diagonal-tolerance <n>   Extra columns accepted in diagonal views (default 0)\n"
        "  --min-fragment <n>         Drop carved fragments smaller than n voxels (default 8)\n"
        "  --no-sheets                Skip PNG comparison sheets and the HTML report\n"
        "  --help                     Show this message\n",
        program);
}

/* Parse a small non-negative integer argument, rejecting anything else. */
static bool parse_count(const char *text, int maximum, int *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);

    if (end == text || *end != '\0' || parsed < 0 || parsed > maximum) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

int main(int argc, char **argv)
{
    WolfPipelineOptions options;

    options.vswap_path = NULL;
    options.output_directory = "sprite_voxels";
    options.multi_view = wolf_multi_view_default_options();
    options.write_comparison_sheets = true;
    options.only_asset = NULL;

    for (int argument = 1; argument < argc; ++argument) {
        const char *name = argv[argument];
        const char *value = argument + 1 < argc ? argv[argument + 1] : NULL;
        int count;

        if (strcmp(name, "--help") == 0 || strcmp(name, "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(name, "--no-sheets") == 0) {
            options.write_comparison_sheets = false;
            continue;
        }
        if (value == NULL) {
            fprintf(stderr, "error: %s needs a value\n", name);
            return 2;
        }
        ++argument;
        if (strcmp(name, "--vswap") == 0) {
            options.vswap_path = value;
        } else if (strcmp(name, "--output") == 0) {
            options.output_directory = value;
        } else if (strcmp(name, "--only") == 0) {
            options.only_asset = value;
        } else if (strcmp(name, "--carve-misses") == 0 && parse_count(value, 7, &count)) {
            options.multi_view.allowed_silhouette_misses = count;
        } else if (strcmp(name, "--diagonal-tolerance") == 0 && parse_count(value, 4, &count)) {
            options.multi_view.diagonal_tolerance_columns = count;
        } else if (strcmp(name, "--min-fragment") == 0 && parse_count(value, 100000, &count)) {
            options.multi_view.minimum_fragment_voxels = (size_t)count;
        } else {
            fprintf(stderr, "error: unknown option or invalid value: %s %s\n", name, value);
            return 2;
        }
    }

    if (options.vswap_path == NULL) {
        print_usage(argv[0]);
        return 2;
    }
    return wolf_pipeline_run(&options);
}
