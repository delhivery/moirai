import { Construct } from "@aws-cdk/core";
import { Artifact, Pipeline } from "@aws-cdk/aws-codepipeline";
import {
  CodeBuildAction,
  S3SourceAction,
  S3Trigger,
} from "@aws-cdk/aws-codepipeline-actions";
import { PipelineProject } from "@aws-cdk/aws-codebuild";
import { Bucket } from "@aws-cdk/aws-s3";
import { Application, StackProps } from "@delhivery/utilities";

export interface DeploymentStackProps extends StackProps {
  pipelineStorage: Bucket;
  searchDockerImagePipeline: PipelineProject;
}

export default class BuildImagePipeline extends Application {
  constructor(scope: Construct, id: string, props: DeploymentStackProps) {
    super(scope, id, props);

    const s3Artifact = new Artifact("source");

    const s3Action = new S3SourceAction({
      bucket: props.pipelineStorage,
      bucketKey: "source.zip",
      actionName: "S3Source",
      runOrder: 1,
      output: s3Artifact,
      trigger: S3Trigger.POLL,
    });

    const pipeline = new Pipeline(this, "pipeline", {
      pipelineName: "pipeline",
      artifactBucket: props.pipelineStorage,
      stages: [
        {
          stageName: "Source",
          actions: [s3Action],
        },
        {
          stageName: "Build",
          actions: [
            new CodeBuildAction({
              actionName: "BuildSearchDockerImage",
              input: s3Artifact,
              project: props.searchDockerImagePipeline,
              runOrder: 1,
            }),
          ],
        },
      ],
    });

    props.pipelineStorage.grantReadWrite(pipeline.role);
  }
}
