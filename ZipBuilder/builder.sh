dirCheck=( "kernel" "META-INF" "META-INF/com" "META-INF/com/google" "META-INF/com/google/android" "system/" "system/lib" "system/lib/modules" )

for check in "${dirCheck[@]}"
	do
		if [ -d $check ]
			then
				echo "Directory Check: OK - '$check'"
			else
				echo "Directory Check: FAIL - '$check' does not exist!!"
				#echo "EXITING - check directory and file contents"
				exit 1
		fi
done

fileCheck=("kernel/dump_image" "kernel/mkbootimg" "kernel/mkbootimg.sh" "kernel/unpackbootimg" "META-INF/com/google/android/update-binary" "META-INF/com/google/android/updater-script" )
for check in "${fileCheck[@]}"
	do
		if [ -e $check ]
			then
				echo "File Check: OK - '$check'"
			else
				echo "File Check: FAIL - '$check' does not exist!!"
				#echo "EXITING - check directory and file contents"
				exit 1
		fi
done


if [ -f ./kernel/zImage ] 
then 
  echo zImage exists!
  rm  ./kernel/zImage
  echo Removed!
  cp ../arch/arm/boot/zImage ./kernel/
else
  cp ../arch/arm/boot/zImage ./kernel/
fi

if [ -f ./system/lib/modules/bcm4329.ko ] 
then 
  echo bcm4329.ko exists! 
  rm ./system/lib/modules/bcm4329.ko
  echo Removed!
  cp ../drivers/net/wireless/bcm4329/bcm4329.ko ./system/lib/modules/
else
  cp ../drivers/net/wireless/bcm4329/bcm4329.ko ./system/lib/modules/
fi

if [ -f ./update.zip ] 
then 
  echo update.zip exists! 
  rm ./update.zip
  echo Removed!
  zip -r -0 update ./system ./META-INF ./kernel
else
  zip -r -0 update ./system ./META-INF ./kernel
fi

if [ -f ./update-signed.zip ] 
  then 
  echo update-singed.zip exists!
  rm ./update-signed.zip
  echo Removed!
  java -jar signapk.jar testkey.x509.pem testkey.pk8 update.zip update_signed.zip
else
  java -jar signapk.jar testkey.x509.pem testkey.pk8 update.zip update_signed.zip
fi

echo Done!
