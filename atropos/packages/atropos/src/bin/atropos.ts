#!/usr/bin/env node
import { App, Tag, Tags } from "@aws-cdk/core";
import { getConstructId, Pipeline, Stage } from "@delhivery/utilities";
import {
  EnvironmentType,
  StackProps,
} from "@delhivery/utilities/dist/cdk/typedefs";
import { URL } from "url";
import ImageRepository from "../stacks/imageRepository";
import SearchDockerImage from "../stacks/searchDockerImage";

const app = new App();
const props: StackProps = {
  project: "moirai",
  component: "atropos",
  technologyUnit: "core",
  environment: EnvironmentType.PRODUCTION,
  uri: new URL("https://github.com/delhivery/moirai"),
};
const pipeline = new Pipeline(app, "atropos", props);
pipeline.output.stackDeploymentPipeline.addApplicationStage(
  new Stage(pipeline, getConstructId("stageAtropos", props), {
    ...props,
    stacks: [ImageRepository, SearchDockerImage],
  })
);
Tags.of(app).add("Project", props.project);
Tags.of(app).add("Component", props.component);
Tags.of(app).add("TechnologyUnit", props.technologyUnit);
Tags.of(app).add("Environment", props.environment);
app.synth();

// const ECR_STACK = new ECRStack(app, "ecr", {});
// new DeploymentPipelineStack(app, "deployment-pipeline", {
//   bucket: ECR_STACK.output.bucket,
//   build: ECR_STACK.output.build,
// });
