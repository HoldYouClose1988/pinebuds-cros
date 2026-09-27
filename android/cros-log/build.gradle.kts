plugins {
    // AGP 9.0 requires Gradle ≥9.1 — needed to run the daemon on JDK 25.
    id("com.android.application") version "9.0.1" apply false
    id("org.jetbrains.kotlin.android") version "2.2.10" apply false
}
