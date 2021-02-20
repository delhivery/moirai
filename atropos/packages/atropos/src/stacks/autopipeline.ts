import { Artifact } from "@aws-cdk/aws-codepipeline";
import {
  GitHubSourceAction,
  GitHubTrigger,
} from "@aws-cdk/aws-codepipeline-actions";
import { Construct, Stack } from "@aws-cdk/core";
import { CdkPipeline, SimpleSynthAction } from "@aws-cdk/pipelines";
import {
  getBranch,
  IBaseProps,
  getConstructId,
  resolveGithubRepository,
} from "@delhivery/utilities";

export default class Pipeline extends Stack {
  constructor(scope: Construct, id: string, props: IBaseProps) {
    super(scope, id, props);

    const sourceArtifact = new Artifact();
    const cloudAssemblyArtifact = new Artifact();

    const stackDeploymentPipeline = new CdkPipeline(
      this,
      getConstructId("githubAutoDeploy", CdkPipeline, {}),
      {
        pipelineName: getConstructId("githubAutoDeploy", CdkPipeline, props),
        cloudAssemblyArtifact,
        crossAccountKeys: false,
        sourceAction: new GitHubSourceAction({
          actionName: "GitSource",
          branch: getBranch(props),
          output: sourceArtifact,
          trigger: GitHubTrigger.WEBHOOK,
          ...resolveGithubRepository(props.uri),
        }),
        synthAction: SimpleSynthAction.standardYarnSynth({
          sourceArtifact,
          cloudAssemblyArtifact,
          buildCommand: "yarn build",
          subdirectory: "atropos",
        }),
      }
    );

    stackDeploymentPipeline.addApplicationStage();
  }
}
