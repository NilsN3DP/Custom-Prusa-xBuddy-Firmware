#!/usr/bin/env python3
import argparse
import polib


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile gettext .po files to .mo files.")
    parser.add_argument("input")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()

    po = polib.pofile(args.input)
    po.save_as_mofile(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
