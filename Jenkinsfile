#!groovy
/*
stage('build linux')
{
	node('linux') {
		try 
		{
			notifyBuild('STARTED')

			checkout scm

			sh '''rm -rf build
				mkdir build
				cd build
				cmake -DCMAKE_BUILD_TYPE="Debug" ..
				make all 
				ctest -V --output-on-failure'''
		}
		catch (e)
		{
			currentBuild.result = "FAILED"
		}
		finally
		{
			notifyBuild(currentBuild.result)
		}
	}
}*/
stage('build windows')
{
	node('windows') {
		try 
		{
			notifyBuild('STARTED')

			checkout scm

			bat '''"C:/Program Files (x86)/Microsoft Visual Studio/2017/BuildTools/Common7/Tools/VsDevCmd.bat"
			        rmdir /S/Q build
				mkdir build
				cd build
				cmake -DCMAKE_BUILD_TYPE="Release" ..
				cmake --build . --target all --config Debug
				ctest -V --output-on-failure'''
		}
		catch (e)
		{
			currentBuild.result = "FAILED"
		}
		finally
		{
			notifyBuild(currentBuild.result)
		}
	}
}

def notifyBuild(String buildStatus = 'STARTED') {
  // build status of null means successful
  buildStatus =  buildStatus ?: 'SUCCESSFUL'
 
  // Default values
  def colorName = 'RED'
  def colorCode = '#FF0000'
  def subject = "${buildStatus}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'"
  def summary = "${subject} (${env.BUILD_URL})"
  def details = '${SCRIPT, template="groovy-text.template"}'
 
  // Override default values based on build status
  if (buildStatus == 'STARTED') {
    color = 'YELLOW'
    colorCode = '#FFFF00'
  } else if (buildStatus == 'SUCCESSFUL') {
    color = 'GREEN'
    colorCode = '#00FF00'
  } else {
    color = 'RED'
    colorCode = '#FF0000'
  }
 
  // Send notifications
 
  emailext (
      subject: subject,
      body: details,
      recipientProviders: [[$class: 'DevelopersRecipientProvider'],[$class: 'RequesterRecipientProvider']]
    )
}
