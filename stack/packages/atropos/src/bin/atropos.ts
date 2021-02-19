#!/usr/bin/env node
import { App } from "@aws-cdk/core";
import ECRStack from "src/stacks/ecr_stack";
import DeploymentPipelineStack from "src/stacks/pipeline";

const app = new App();
const ECR_STACK = new ECRStack(app, "ecr", {});
new DeploymentPipelineStack(app, "deployment-pipeline", {
  bucket: ECR_STACK.output.bucket,
  build: ECR_STACK.output.build,
});
