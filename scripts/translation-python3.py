# xgettext --keyword=_ --keyword="_P:1,2" --language=C --from-code=UTF-8 -o ../infclass-translation/infclasspot.po $(find ./src -name \*.cpp -or -name \*.h)

import json
import os
import sys
from dataclasses import dataclass
from typing import List

try:
    import polib
except ModuleNotFoundError:
    print(
        "The Python library 'polib' is required and can be installed via the 'python3-polib' or 'python-polib' package."
    )
    sys.exit(1)

if len(sys.argv) != 3:
    print(f"Expected 2 arguments, got {len(sys.argv) - 1}")
    sys.exit(1)

PO_LANG_DIR = sys.argv[1]
TARGET_DIR = os.path.join(sys.argv[2], "data/languages")

if not os.path.exists(TARGET_DIR):
    os.makedirs(TARGET_DIR)


def convert_po_to_json(language: "Language"):
    language_code = language.code
    plurals = language.plurals.copy()
    plurals.append("other")

    po_file_name = os.path.join(PO_LANG_DIR, f"{language_code}/infclass.po")
    if os.path.isfile(po_file_name):
        json_file_name = os.path.join(TARGET_DIR, f"{language_code}.json")

        po = polib.pofile(po_file_name)

        with open(json_file_name, "w", encoding="utf-8") as f:
            target_dict = {"translation": []}
            translations = target_dict["translation"]
            for entry in po:
                if entry.fuzzy or entry.obsolete:
                    continue
                if entry.msgstr:
                    target_entry = {"key": entry.msgid, "value": entry.msgstr}
                elif entry.msgstr_plural.keys():
                    target_entry = {"key": entry.msgid_plural}
                    for i in sorted(entry.msgstr_plural.keys()):
                        if entry.msgstr_plural[i]:
                            target_entry[plurals[i]] = entry.msgstr_plural[i]
                    if len(target_entry) <= 1:
                        continue
                    if entry.msgid:
                        one_entry = target_entry.copy()
                        one_entry["key"] = entry.msgid
                        translations.append(one_entry)
                else:
                    continue
                translations.append(target_entry)

            json.dump(target_dict, f, ensure_ascii=False, separators=(",", ":"))


@dataclass
class Language:
    code: str
    plurals: List[str]


languages: List[Language] = []
output_index = []

with open(os.path.join(PO_LANG_DIR, "index.json"), encoding="utf-8") as f:
    index = json.load(f)

for lang_index in index["languages"]:
    output_lang_index = {}
    output_lang_index["file"] = lang_index["file"]
    output_lang_index["name"] = lang_index["name"]
    if "parent" in lang_index:
        output_lang_index["parent"] = lang_index["parent"]
    if "direction" in lang_index:
        output_lang_index["direction"] = lang_index["direction"]
    output_index.append(output_lang_index)

    if lang_index["file"] == "en":
        continue

    languages.append(Language(lang_index["file"], lang_index["plurals"]))

with open(os.path.join(TARGET_DIR, "index.json"), "w", encoding="utf-8") as f:
    json.dump({"language indices": output_index}, f, ensure_ascii=False, separators=(",", ":"))

for language in languages:
    convert_po_to_json(language)
