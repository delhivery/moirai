import { Construct, Duration, RemovalPolicy } from "@aws-cdk/core";
import { Repository } from "@aws-cdk/aws-ecr";
import { BuildSpec, PipelineProject } from "@aws-cdk/aws-codebuild";
import { Bucket } from "@aws-cdk/aws-s3";
// import { StringParameter } from "@aws-cdk/aws-ssm";
import { Application, getConstructId, StackProps } from "@delhivery/utilities";

export interface SearchDockerImageProps extends StackProps {
  dockerImageRepository: Repository;
}

/**
 * Specifies how to build a docker image for Elasticsearch
 */
export default class SearchDockerImage extends Application {
  constructor(scope: Construct, id: string, props: SearchDockerImageProps) {
    super(scope, id, props);

    const pipelineStorage = new Bucket(
      this,
      getConstructId("pipelineStorage", props),
      {
        bucketName: getConstructId("pipelineStorage", props),
        versioned: true,
        removalPolicy: RemovalPolicy.DESTROY,
      }
    );

    /* const pipelineStorageParameters = new StringParameter(
      this,
      getConstructId("pipelineStorageParameters", props),
      {
        parameterName: getConstructId("pipelineStorageParameters", props),
        stringValue: pipelineStorage.bucketName,
        description: "Docker build pipeline storage bucket",
      }
    ); */

    const searchDockerImagePipeline = new PipelineProject(
      this,
      getConstructId("searchDockerImagePipeline", props),
      {
        projectName: getConstructId("searchDockerImagePipeline", props),
        buildSpec: BuildSpec.fromSourceFilename(
          "assets/search/build/dockerfile.yml"
        ),
        environment: {
          privileged: true,
        },
        environmentVariables: {
          ecr: {
            value: props.dockerImageRepository.repositoryUri,
          },
          tag: { value: "cdk" },
        },
        description: "Build pipeline to build elasticsearch docker images",
        timeout: Duration.minutes(10),
      }
    );
    pipelineStorage.grantReadWrite(searchDockerImagePipeline);
    props.dockerImageRepository.grantPullPush(searchDockerImagePipeline);

    this.output.pipelineStorage = pipelineStorage;
    this.output.searchDockerImagePipeline = searchDockerImagePipeline;
  }
}
