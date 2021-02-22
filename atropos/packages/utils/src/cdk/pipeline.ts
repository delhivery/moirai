import { Artifact } from "@aws-cdk/aws-codepipeline";
import {
  GitHubSourceAction,
  GitHubTrigger,
} from "@aws-cdk/aws-codepipeline-actions";
import { Construct as CdkConstruct } from "@aws-cdk/core";
import { CdkPipeline, SimpleSynthAction } from "@aws-cdk/pipelines";
import { getBranch, getConstructId, resolveGithubRepository } from "./shared";
import { StackProps } from "./typedefs";
import Application from "./application";

export default class Pipeline extends Application {
  constructor(scope: CdkConstruct, id: string, props: StackProps) {
    super(scope, id, props);

    const sourceArtifact = new Artifact();
    const cloudAssemblyArtifact = new Artifact();

    const stackDeploymentPipeline = new CdkPipeline(
      this,
      getConstructId("githubAutoDeploy", props),
      {
        pipelineName: getConstructId("githubAutoDeploy", props),
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
    this.output.stackDeploymentPipeline = stackDeploymentPipeline;

    /* stackDeploymentPipeline.addApplicationStage(
      new Stage(this, getConstructId("applicationStage", props), props)
    ); */
  }
}
