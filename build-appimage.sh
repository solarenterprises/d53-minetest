#!/bin/bash

appimage-builder --recipe AppImageBuilder.yml

# Path to the AppImage and download folder
app_image_path="$(pwd)"  # Adjust this path to where the AppImages are stored
download_path="/var/www/html/downloads"

# Find the latest AppImage version using pattern matching for version numbers
latest_app_image=$(ls -1 $app_image_path/District53-*-x86_64.AppImage | sort -V | tail -n 1)

# Find the highest build number in the existing zip files
max_build_number=$(ls -1 $download_path/build-*-linux.zip | sed 's/.*build-\([0-9]*\)-linux.zip/\1/' | sort -nr | head -n 1)

# Increment the build number by 1 for the new folder
new_build_number=$((max_build_number + 1))

# Create a new folder for the next build
new_folder="build-${new_build_number}-linux"
mkdir $new_folder

# Copy the latest AppImage into the new folder
cp $latest_app_image $new_folder

zip_name=${new_folder}.zip
# Zip the new folder
zip -r ${zip_name} $new_folder

# Change the ownership to www-data
sudo chown -R www-data:www-data ${zip_name}

cp ${zip_name} ${download_path}/
rm ${zip_name}
rm -r $new_folder

echo "Process completed. The new build is located at $download_path/${new_folder}.zip"
