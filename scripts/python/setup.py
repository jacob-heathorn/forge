#!/usr/bin/env python3
#
# Setup tool.

# System pythonmodules
import argparse
import git
import os
import shutil

# Custom imports
from forge.helpers import error

# Pull in environment variables
FORGE_ROOT = os.environ.get("FORGE_ROOT")
FORGE_CACHE = os.path.join(FORGE_ROOT, 'cache')

# def download_file(url, local_path):
#   # Check if the file exists
#   if os.path.exists(local_path):
#       print(f"The file {local_path} already exists. Skipping download.")
#       return
  
#   # Download the file
#   print(f"Downloading {url} to {local_path}...")
#   response = requests.get(url)
  
#   # Check if the download was successful
#   if response.status_code == 200:
#       with open(local_path, 'wb') as f:
#           f.write(response.content)
#       print(f"Download complete.")
#   else:
#       print(f"Failed to download the file. HTTP Status Code: {response.status_code}")


def clone_and_checkout(repo_url, local_path, tag):
  # Check if the repository already exists
  if os.path.isdir(local_path):
    # If it does, navigate into it and checkout
    repo = git.Repo(local_path)

    # Error if we've made uncommitted changes
    if repo.is_dirty():
      error(f" - {local_path} has uncommitted changes")

    # Get the commit hash of HEAD
    head_commit = repo.head.commit.hexsha


    # Loop through all tags and check if any of their commit hashes match HEAD
    for tag_ref in repo.tags:
      if tag_ref.name == tag:
        if tag_ref.commit.hexsha == head_commit:
          print(f" - {tag} already checked out")
        else:
          repo.git.checkout('tags/' + tag)
          print(f" - Checked out {tag}")

    repo.git.checkout('tags/' + tag)
  else:
    # If it doesn't exist, clone and navigate into it
    print(f" - Cloning {repo_url}...")
    repo = git.Repo.clone_from(repo_url, local_path)
    print(" - Clone complete.")

    # Check out the specified tag
    print(f" - Checking out tag {tag}...")
    repo.git.checkout('tags/' + tag)

  
def setup_googletest():
  print("Setting up googletest...")
  clone_and_checkout('https://github.com/google/googletest', f'{FORGE_CACHE}/googletest', 'v1.13.0')
  print(" - success")



def main():
  parser = argparse.ArgumentParser(description='Repository build driver')
  parser.add_argument('-c', '--clean', action='store_true', default=False, help='Clean/uninstall')
  parser.add_argument('-i', '--install', action='store_true', default=False, help='Do the setup installations')


  args = parser.parse_args()
  # print("gello")
  # download_file()
  # Repo.clone_from('https://github.com/google/googletest', f'{FORGE_ROOT}/cache/')
  # clone_and_checkout(repo_url, local_path, tag)

  # Do clean
  if args.clean:
    if os.path.exists(FORGE_CACHE):
      shutil.rmtree(FORGE_CACHE)

  # Create cache if it dne
  if not os.path.exists(FORGE_CACHE):
    os.mkdir(FORGE_CACHE)

  # Googletest
  if args.install:
    setup_googletest()
     

if __name__ == '__main__':
  main()
