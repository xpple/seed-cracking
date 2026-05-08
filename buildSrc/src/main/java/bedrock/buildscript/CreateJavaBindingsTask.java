package bedrock.buildscript;

import org.apache.tools.ant.taskdefs.condition.Os;
import org.gradle.api.tasks.Exec;

public abstract class CreateJavaBindingsTask extends Exec {

    private static final String EXTENSION = Os.isFamily(Os.FAMILY_WINDOWS) ? ".bat" : "";
    private static final String JAVA_HOME = System.getProperty("java.home");
    private static final String OS_INCLUDE_DIR;

    static {
        if (Os.isFamily(Os.FAMILY_UNIX)) { // does not include MacOS
            OS_INCLUDE_DIR = JAVA_HOME + "/include/linux";
        } else if (Os.isFamily(Os.FAMILY_MAC)) {
            OS_INCLUDE_DIR = JAVA_HOME + "/include/darwin";
        } else if (Os.isFamily(Os.FAMILY_WINDOWS)) {
            OS_INCLUDE_DIR = JAVA_HOME + "/include/win32";
        } else {
            throw new AssertionError("Unknown OS");
        }
    }

    {
        // always run task
        this.getOutputs().upToDateWhen(_ -> false);

        this.setWorkingDir(this.getProject().getRootDir());
        this.setStandardOutput(System.out);
        this.commandLine("./jextract/build/jextract/bin/jextract" + EXTENSION, "--include-dir", "src/main/c", "--include-dir", JAVA_HOME + "/include", "--include-dir", OS_INCLUDE_DIR, "--output", "src/main/java", "--use-system-load-library", "--target-package", "bedrock.generated", "--header-class-name", "BedrockNative", "@includes.txt", "blocks.h");
    }
}
