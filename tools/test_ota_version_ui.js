"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

const src = fs.readFileSync(path.join(__dirname, "..", "cdn", "DSMRindex.js"), "utf8");
const begin = src.indexOf("// OTA_VERSION_BEGIN");
const end = src.indexOf("// OTA_VERSION_END");
if (begin < 0 || end < 0 || end <= begin) {
  console.error("DSMRindex.js is missing OTA_VERSION_BEGIN/END helpers");
  process.exit(1);
}

const context = { console };
vm.createContext(context);
vm.runInContext(src.slice(begin, end + "// OTA_VERSION_END".length), context);

const { parseOtaVersionParts, otaVersionIsNewer } = context;
const cases = [
  [otaVersionIsNewer(parseOtaVersionParts("5.8.4.1"), parseOtaVersionParts("5.8.4")), true, "5.8.4.1 > 5.8.4"],
  [otaVersionIsNewer(parseOtaVersionParts("5.8.4.1"), parseOtaVersionParts("5.8.4.1")), false, "equal"],
  [otaVersionIsNewer(parseOtaVersionParts("5.8.4.2"), parseOtaVersionParts("5.8.4.1")), true, "5.8.4.2 > 5.8.4.1"],
  [otaVersionIsNewer(parseOtaVersionParts("5.8.4.1", 1), parseOtaVersionParts("5.8.4")), true, "fork field"],
  [otaVersionIsNewer(parseOtaVersionParts("5.8.13"), parseOtaVersionParts("5.8.4.1")), true, "vendor patch wins"],
  [otaVersionIsNewer(parseOtaVersionParts("5.8.4.9"), parseOtaVersionParts("5.9.0")), false, "minor wins over fork"],
];

let failed = 0;
for (const [got, want, name] of cases) {
  if (got !== want) {
    console.error(`FAIL: ${name} (got ${got} want ${want})`);
    failed++;
  }
}
process.exit(failed ? 1 : 0);
