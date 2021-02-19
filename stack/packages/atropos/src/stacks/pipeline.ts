import { Construct, Stack, StackProps } from "@aws-cdk/core";
import { Artifact, Pipeline } from "@aws-cdk/aws-codepipeline";
import {
  CodeBuildAction,
  S3SourceAction,
  S3Trigger,
} from "@aws-cdk/aws-codepipeline-actions";
import { PipelineProject } from "@aws-cdk/aws-codebuild";
import { Bucket } from "@aws-cdk/aws-s3";
import { StringParameter } from "@aws-cdk/aws-ssm";

export interface DeploymentStackProps extends StackProps {
  bucket: Bucket;
  build: PipelineProject;
}

export default class DeploymentPipelineStack extends Stack {
  constructor(scope: Construct, id: string, props: DeploymentStackProps) {
    super(scope, id, props);

    const s3Artifact = new Artifact("source");

    const s3Action = new S3SourceAction({
      bucket: props.bucket,
      bucketKey: "source.zip",
      actionName: "S3Source",
      runOrder: 1,
      output: s3Artifact,
      trigger: S3Trigger.POLL,
    });

    const pipeline = new Pipeline(this, "pipeline", {
      pipelineName: "pipeline",
      artifactBucket: props.bucket,
      stages: [
        {
          stageName: "Source",
          actions: [s3Action],
        },
        {
          stageName: "Build",
          actions: [
            new CodeBuildAction({
              actionName: "BuildDockerImage",
              input: s3Artifact,
              project: props.build,
              runOrder: 1,
            }),
          ],
        },
      ],
    });

    props.bucket.grantReadWrite(pipeline.role);

    const pipeline_params = new StringParameter(this, "pipline_params", {
      parameterName: "pipeline",
      stringValue: pipeline.pipelineName,
      description: "cdk pipeline name",
    });
  }
}
