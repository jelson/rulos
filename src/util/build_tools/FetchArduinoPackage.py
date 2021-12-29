#!/usr/bin/env python3

import hashlib
import json
import os
import requests
import shutil
import subprocess
import sys

class Fetcher:
    def __init__(self, url, package_name, version, host_platform, dest_dir):
        self.url = url
        self.package_name = package_name
        self.version = version
        self.host_platform = host_platform
        self.dest_dir = dest_dir

    def fetch(self):
        if not os.path.exists(self.dest_dir):
            os.makedirs(self.dest_dir)

        self.package_index = json.loads(requests.get(self.url).text)

        for fetchable in self.select_package():
            self.fetch_component(fetchable)

    def select_package(self):
        # Find the package matching the package name we were asked to retrieve
        packages = [p for p in self.package_index["packages"] if p['name'] == self.package_name]
        assert len(packages) == 1, f"Found {len(packages)} {self.package_name} packages when we expected 1"
        package = packages[0]

        # Find the right version of that package
        versions = [v for v in package['platforms'] if v['version'] == self.version]
        assert len(versions) == 1, f"Found {len(versions)} versions of {self.package_name} v{self.version} when we expected 1"
        version = versions[0]

        return [version] + [self.select_tools_dependencies(tool) for tool in version["toolsDependencies"]]

    # Returns a single fetchable object
    def select_tools_dependencies(self, spec):
        tools = self.package_index["packages"][0]["tools"]
        tool_match = [t for t in tools
            if t["name"]==spec["name"] and t["version"]==spec["version"]]
        assert len(tool_match)==1, f"Found {len(tool_match)} matches for {spec['name']}, {spec['version']}"
        tool = tool_match[0]

        system_match = [s for s in tool["systems"]
            if s["host"] == self.host_platform]
        assert len(system_match)==1, f"Found {len(system_match)} matches for {tool['name']}, {self.host_platform}"
        system = system_match[0]
        system["name"] = tool["name"]
        system["version"] = tool["version"]
        return system

    def fetch_component(self, fetchable):
        url = fetchable["url"]
        archiveFileName = fetchable["archiveFileName"]
        checksum = fetchable["checksum"]

        # fetch the archive url
        local_filename = os.path.join(self.dest_dir, archiveFileName)
        print(f"Fetching {url}")
        with requests.get(url, stream=True) as r:
            with open(local_filename, 'wb') as f:
                shutil.copyfileobj(r.raw, f)
        checktype,expected_hash = fetchable["checksum"].split(":")
        assert checktype=="SHA-256", "I only support sha256"

        # check the hash
        sha256_hash = hashlib.sha256()
        with open(local_filename,"rb") as f:
            for byte_block in iter(lambda: f.read(4096),b""):
                sha256_hash.update(byte_block)
        found_hash = sha256_hash.hexdigest()
        assert found_hash == expected_hash, "Checksums don't match"

        # Expand the archive
        expand_dir = os.path.join(self.dest_dir, fetchable["name"])
        if os.path.exists(expand_dir):
            shutil.rmtree(expand_dir)
        os.makedirs(expand_dir)
        if archiveFileName.endswith(".zip"):
            subprocess.call(["unzip", "-qq", "-d", expand_dir, local_filename])
        elif archiveFileName.endswith(".tar.gz"):
            subprocess.call(["tar", "-C", expand_dir, "-xzf", local_filename])
            pass
        else:
            assert False, "no support for archive type"


if __name__ == "__main__":
    if len(sys.argv) > 1:
        args = sys.argv[1:]
    else:
        args = [
            'https://dl.espressif.com/dl/package_esp32_index.json',
            'esp32',
            '1.0.6',
            'x86_64-pc-linux-gnu',
            '/tmp/build'
        ]

    fetcher = Fetcher(*args)
    fetcher.fetch()
