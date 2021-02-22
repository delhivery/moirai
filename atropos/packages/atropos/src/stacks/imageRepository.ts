import { Repository } from "@aws-cdk/aws-ecr";
import { Construct, RemovalPolicy } from "@aws-cdk/core";
import { Application, getConstructId, StackProps } from "@delhivery/utilities";

/**
 * Creates a repository for docker images
 */
export default class ImageRepository extends Application {
  constructor(scope: Construct, id: string, props: StackProps) {
    super(scope, id, props);

    const dockerImageRepository = new Repository(
      this,
      getConstructId("dockerImageRepository", props),
      {
        imageScanOnPush: true,
        repositoryName: getConstructId("dockerImageRepository", props),
        removalPolicy: RemovalPolicy.DESTROY,
      }
    );
    this.output.dockerImageRepository = dockerImageRepository;
  }
}
