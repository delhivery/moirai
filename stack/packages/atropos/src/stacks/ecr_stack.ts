import {
  CfnOutput,
  Construct,
  Duration,
  RemovalPolicy,
  Stack,
  StackProps,
} from "@aws-cdk/core";
import { Repository } from "@aws-cdk/aws-ecr";
import { BuildSpec, PipelineProject } from "@aws-cdk/aws-codebuild";
import { Bucket } from "@aws-cdk/aws-s3";
import { StringParameter } from "@aws-cdk/aws-ssm";

const getConstructName = (str: string): string => str;

export default class ECRStack extends Stack {
  public readonly output: { [key: string]: any };

  constructor(scope: Construct, id: string, props?: StackProps) {
    super(scope, id, props);
    this.output = {};

    const repositoryStorage = new Bucket(this, getConstructName("search"), {
      bucketName: getConstructName("search"),
      versioned: true,
      removalPolicy: RemovalPolicy.DESTROY,
    });

    const repositoryStorageParameters = new StringParameter(
      this,
      getConstructName("search"),
      {
        parameterName: getConstructName("search"),
        stringValue: repositoryStorage.bucketName,
        description: "Docker build pipeline storage bucket",
      }
    );

    const repository = new Repository(this, getConstructName("search"), {
      imageScanOnPush: true,
      repositoryName: getConstructName("search"),
      removalPolicy: RemovalPolicy.DESTROY,
    });

    const searchImageBuild = new PipelineProject(
      this,
      getConstructName("search"),
      {
        projectName: getConstructName("search"),
        buildSpec: BuildSpec.fromSourceFilename(
          "assets/search/build/dockerfile.yml"
        ),
        environment: {
          privileged: true,
        },
        environmentVariables: {
          ecr: {
            value: repository.repositoryUri,
          },
          tag: { value: "cdk" },
        },
        description: "Build pipeline to build elasticsearch docker images",
        timeout: Duration.minutes(10),
      }
    );
    repositoryStorage.grantReadWrite(searchImageBuild);
    repository.grantPullPush(searchImageBuild);

    const stackURIOutputs = new CfnOutput(this, getConstructName("search"), {
      description: "ECR Uri",
      value: repository.repositoryUri,
    });

    const stackStorageOutputs = new CfnOutput(
      this,
      getConstructName("search"),
      {
        description: "Docker image pipeline bucket",
        value: repositoryStorage.bucketName,
      }
    );

    this.output.bucket = repositoryStorage;
    this.output.build = searchImageBuild;
    this.output.params = repositoryStorageParameters;
    this.output.outputs = [stackURIOutputs, stackStorageOutputs];
  }
}
