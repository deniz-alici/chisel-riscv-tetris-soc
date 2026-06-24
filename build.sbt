ThisBuild / version          := "0.1.0-SNAPSHOT"
ThisBuild / organization     := "com.github.deniz-alici"
ThisBuild / scalaVersion     := "2.13.12"

lazy val root = (project in file("."))
  .settings(
    name := "chisel-riscv-tetris-soc",
    libraryDependencies ++= Seq(
      "org.chipsalliance" %% "chisel" % "6.0.0"
    ),
    scalacOptions ++= Seq(
      "-language:reflectiveCalls",
      "-deprecation",
      "-feature",
      "-Xcheckinit",
    ),
    addCompilerPlugin("org.chipsalliance" % "chisel-plugin" % "6.0.0" cross CrossVersion.full),
  )
